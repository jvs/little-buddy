#include <bsp/board_api.h>
#include <hardware/i2c.h>
#include <hardware/gpio.h>
#include <pico/stdlib.h>
#include <stdbool.h>
#include <tusb.h>
#include <pio_usb.h>

#include "sh1107_display.h"

uint32_t cdc_task(void);
void hid_task(void);

int main() {
    // Give everything time to power up properly.
    sleep_ms(2000);

    // Initialize board
    board_init();

    // Initialize I2C for STEMMA QT connector (GPIO 2=SDA, 3=SCL)
    sleep_ms(500);
    i2c_init(i2c1, 400000);
    gpio_set_function(2, GPIO_FUNC_I2C);
    gpio_set_function(3, GPIO_FUNC_I2C);
    gpio_pull_up(2);
    gpio_pull_up(3);

    // Initialize display
    sh1107_t display;
    bool display_ok = sh1107_init(&display, i2c1);

    // Show initial startup message
    if (display_ok) {
        sh1107_clear(&display);
        sh1107_draw_string(&display, 2, 10, "STARTING...");
        sh1107_display(&display);
        sleep_ms(500);
    }

    // Initialize TinyUSB device stack
    tud_init(BOARD_TUD_RHPORT);

    // Initialize PIO USB for host mode
    pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
    pio_cfg.pin_dp = PICO_DEFAULT_PIO_USB_DP_PIN;
    tuh_configure(BOARD_TUH_RHPORT, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &pio_cfg);

    // Initialize TinyUSB host stack
    tuh_init(BOARD_TUH_RHPORT);

    // Wait a bit for USB to initialize
    sleep_ms(100);

    // Show USB ready message
    if (display_ok) {
        sh1107_clear(&display);
        sh1107_draw_string(&display, 2, 10, "USB DUAL MODE");
        sh1107_draw_string(&display, 2, 25, "DEVICE + HOST");
        sh1107_display(&display);
    }

    static uint32_t byte_count = 0;
    char status_buf[32];

    while (1) {
        // TinyUSB device task
        tud_task();

        // TinyUSB host task
        tuh_task();

        // CDC task for handling serial communication
        uint32_t old_count = byte_count;
        byte_count += cdc_task();

        // HID host task for handling connected devices
        hid_task();

        // Update display when we receive data
        if (byte_count != old_count && display_ok) {
            sh1107_clear(&display);
            sh1107_draw_string(&display, 2, 10, "USB DUAL MODE");
            snprintf(status_buf, sizeof(status_buf), "DEV BYTES: %lu", byte_count);
            sh1107_draw_string(&display, 2, 25, status_buf);
            sh1107_display(&display);
        }
    }

    return 0;
}

uint32_t cdc_task(void) {
    uint32_t bytes_received = 0;
    if (tud_cdc_available()) {
        uint8_t buf[64];
        uint32_t count = tud_cdc_read(buf, sizeof(buf));

        // Send back a clear response
        char response[64];
        snprintf(response, sizeof(response), "GOT %lu BYTES\r\n", count);
        tud_cdc_write_str(response);
        tud_cdc_write_flush();

        bytes_received = count;
    }
    return bytes_received;
}

// Device Descriptor
tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = 0xCafe,
    .idProduct          = 0x4001,
    .bcdDevice          = 0x0100,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01
};

uint8_t const * tud_descriptor_device_cb(void) {
    return (uint8_t const *) &desc_device;
}

// Configuration Descriptor
enum {
    ITF_NUM_CDC = 0,
    ITF_NUM_CDC_DATA,
    ITF_NUM_TOTAL
};

#define CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN)
#define EPNUM_CDC_NOTIF   0x81
#define EPNUM_CDC_OUT     0x02
#define EPNUM_CDC_IN      0x82

uint8_t const desc_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 4, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, EPNUM_CDC_IN, 64),
};

uint8_t const * tud_descriptor_configuration_cb(uint8_t index) {
    (void) index;
    return desc_configuration;
}

// String Descriptors
char const* string_desc_arr [] = {
    (const char[]) { 0x09, 0x04 }, // 0: language
    "Little Buddy",                // 1: Manufacturer
    "USB Device",                  // 2: Product
    "123456",                      // 3: Serials
    "CDC Interface",               // 4: CDC Interface
};

static uint16_t _desc_str[32];

uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void) langid;

    uint8_t chr_count;

    if (index == 0) {
        memcpy(&_desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
    } else {
        if (!(index < sizeof(string_desc_arr)/sizeof(string_desc_arr[0]))) return NULL;

        const char* str = string_desc_arr[index];

        chr_count = strlen(str);
        if (chr_count > 31) chr_count = 31;

        for(uint8_t i=0; i<chr_count; i++) {
            _desc_str[1+i] = str[i];
        }
    }

    _desc_str[0] = (TUSB_DESC_STRING << 8 ) | (2*chr_count + 2);

    return _desc_str;
}

//--------------------------------------------------------------------+
// HOST MODE CALLBACKS
//--------------------------------------------------------------------+

void tuh_mount_cb(uint8_t dev_addr) {
    char msg[64];
    snprintf(msg, sizeof(msg), "HOST: Device attached, addr=%d\r\n", dev_addr);
    tud_cdc_write_str(msg);
    tud_cdc_write_flush();
}

void tuh_umount_cb(uint8_t dev_addr) {
    char msg[64];
    snprintf(msg, sizeof(msg), "HOST: Device removed, addr=%d\r\n", dev_addr);
    tud_cdc_write_str(msg);
    tud_cdc_write_flush();
}

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len) {
    char msg[64];
    snprintf(msg, sizeof(msg), "HOST: HID mounted, addr=%d inst=%d\r\n", dev_addr, instance);
    tud_cdc_write_str(msg);
    tud_cdc_write_flush();

    // Request to receive report
    if (!tuh_hid_receive_report(dev_addr, instance)) {
        tud_cdc_write_str("HOST: Error requesting HID report\r\n");
        tud_cdc_write_flush();
    }
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
    char msg[64];
    snprintf(msg, sizeof(msg), "HOST: HID unmounted, addr=%d inst=%d\r\n", dev_addr, instance);
    tud_cdc_write_str(msg);
    tud_cdc_write_flush();
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
    char msg[128];
    int offset = snprintf(msg, sizeof(msg), "HOST: HID data [%d,%d]: ", dev_addr, instance);

    for(uint16_t i = 0; i < len && offset < sizeof(msg) - 4; i++) {
        offset += snprintf(msg + offset, sizeof(msg) - offset, "%02X ", report[i]);
    }
    offset += snprintf(msg + offset, sizeof(msg) - offset, "\r\n");

    tud_cdc_write_str(msg);
    tud_cdc_write_flush();

    // Continue to request next report
    tuh_hid_receive_report(dev_addr, instance);
}

void hid_task(void) {
    // This function is called from main loop
    // HID host processing is handled in the callbacks above
}
