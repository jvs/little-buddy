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
void send_keyboard_report(uint8_t modifier, uint8_t keycode);
void send_mouse_report(int8_t delta_x, int8_t delta_y, uint8_t buttons);

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

        // Process commands
        for (uint32_t i = 0; i < count; i++) {
            char c = buf[i];
            switch (c) {
                case 'l':
                    // Move mouse left
                    for (int j = 0; j < 10; j++) {
                        send_mouse_report(-5, 0, 0);
                        sleep_ms(20);
                    }
                    tud_cdc_write_str("MOUSE LEFT\r\n");
                    break;
                case 'r':
                    // Move mouse right
                    for (int j = 0; j < 10; j++) {
                        send_mouse_report(5, 0, 0);
                        sleep_ms(20);
                    }
                    tud_cdc_write_str("MOUSE RIGHT\r\n");
                    break;
                case 'u':
                    // Move mouse up
                    for (int j = 0; j < 10; j++) {
                        send_mouse_report(0, -5, 0);
                        sleep_ms(20);
                    }
                    tud_cdc_write_str("MOUSE UP\r\n");
                    break;
                case 'd':
                    // Move mouse down
                    for (int j = 0; j < 10; j++) {
                        send_mouse_report(0, 5, 0);
                        sleep_ms(20);
                    }
                    tud_cdc_write_str("MOUSE DOWN\r\n");
                    break;
                case 'a':
                    // Wait then send 'b' key
                    sleep_ms(500);
                    send_keyboard_report(0, HID_KEY_B);  // Press 'b'
                    sleep_ms(50);
                    send_keyboard_report(0, 0);          // Release 'b'
                    tud_cdc_write_str("SENT KEY 'B'\r\n");
                    break;
                default:
                    // Send back normal response
                    tud_cdc_write_str("GOT 1 BYTES\r\n");
                    break;
            }
        }
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
    .idVendor           = 0x1209,
    .idProduct          = 0x0001,
    .bcdDevice          = 0x0100,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01
};

uint8_t const * tud_descriptor_device_cb(void) {
    return (uint8_t const *) &desc_device;
}

// HID Report Descriptor for Keyboard
uint8_t const desc_hid_report_keyboard[] = {
    TUD_HID_REPORT_DESC_KEYBOARD()
};

// HID Report Descriptor for Mouse
uint8_t const desc_hid_report_mouse[] = {
    TUD_HID_REPORT_DESC_MOUSE()
};

// Configuration Descriptor
enum {
    ITF_NUM_CDC = 0,
    ITF_NUM_CDC_DATA,
    ITF_NUM_HID_KEYBOARD,
    ITF_NUM_HID_MOUSE,
    ITF_NUM_TOTAL
};

#define CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + TUD_HID_DESC_LEN + TUD_HID_DESC_LEN)
#define EPNUM_CDC_NOTIF   0x81
#define EPNUM_CDC_OUT     0x02
#define EPNUM_CDC_IN      0x82
#define EPNUM_HID_KEYBOARD 0x83
#define EPNUM_HID_MOUSE   0x84

uint8_t const desc_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 4, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, EPNUM_CDC_IN, 64),
    TUD_HID_DESCRIPTOR(ITF_NUM_HID_KEYBOARD, 5, HID_ITF_PROTOCOL_KEYBOARD, sizeof(desc_hid_report_keyboard), EPNUM_HID_KEYBOARD, CFG_TUD_HID_EP_BUFSIZE, 5),
    TUD_HID_DESCRIPTOR(ITF_NUM_HID_MOUSE, 6, HID_ITF_PROTOCOL_MOUSE, sizeof(desc_hid_report_mouse), EPNUM_HID_MOUSE, CFG_TUD_HID_EP_BUFSIZE, 5),
};

uint8_t const * tud_descriptor_configuration_cb(uint8_t index) {
    (void) index;
    return desc_configuration;
}

// Invoked when received GET HID REPORT DESCRIPTOR request
uint8_t const * tud_hid_descriptor_report_cb(uint8_t instance) {
    if (instance == 0) {
        return desc_hid_report_keyboard;
    } else {
        return desc_hid_report_mouse;
    }
}

// String Descriptors
char const* string_desc_arr [] = {
    (const char[]) { 0x09, 0x04 }, // 0: language
    "Generic",                     // 1: Manufacturer
    "USB Keyboard",                // 2: Product
    "123456",                      // 3: Serials
    "CDC Interface",               // 4: CDC Interface
    "HID Keyboard",                // 5: HID Keyboard Interface
    "HID Mouse",                   // 6: HID Mouse Interface
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
// HID DEVICE CALLBACKS
//--------------------------------------------------------------------+

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) {
    (void) instance;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) reqlen;

    return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
    (void) instance;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) bufsize;
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

//--------------------------------------------------------------------+
// HID DEVICE HELPER FUNCTIONS
//--------------------------------------------------------------------+

void send_keyboard_report(uint8_t modifier, uint8_t keycode) {
    // Check if keyboard instance is ready
    if (!tud_hid_n_ready(0)) return;

    // Keyboard report format: [key1, key2, key3, key4, key5, key6]
    uint8_t keyreport[6] = {0};
    keyreport[0] = keycode;
    
    tud_hid_n_keyboard_report(0, 0, modifier, keyreport);
}

void send_mouse_report(int8_t delta_x, int8_t delta_y, uint8_t buttons) {
    // Check if mouse instance is ready
    if (!tud_hid_n_ready(1)) return;

    tud_hid_n_mouse_report(1, 0, buttons, delta_x, delta_y, 0, 0);
}
