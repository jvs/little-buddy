#include <bsp/board_api.h>
#include <hardware/i2c.h>
#include <hardware/gpio.h>
#include <pico/stdlib.h>
#include <stdbool.h>
#include <tusb.h>
#include <pio_usb.h>

#include "sh1107_display.h"

// HID device tracking
typedef struct {
    uint8_t dev_addr;
    uint8_t instance;
    uint8_t itf_protocol;  // HID_ITF_PROTOCOL_KEYBOARD, HID_ITF_PROTOCOL_MOUSE, etc.
    bool is_connected;
    uint16_t report_desc_len;
    bool has_keyboard;  // Detected keyboard usage in descriptor
    bool has_mouse;     // Detected mouse usage in descriptor
    uint8_t input_report_size;  // Size of input reports
} hid_device_info_t;

#define MAX_HID_DEVICES 4
static hid_device_info_t hid_devices[MAX_HID_DEVICES];

// HID Descriptor parsing - using raw item values since TinyUSB macros are for generation
#define HID_ITEM_USAGE_PAGE     0x05
#define HID_ITEM_USAGE          0x09
#define HID_ITEM_REPORT_SIZE    0x75
#define HID_ITEM_REPORT_COUNT   0x95
#define HID_ITEM_INPUT          0x81
#define HID_ITEM_COLLECTION     0xA1
#define HID_ITEM_END_COLLECTION 0xC0

// HID Usage Pages (values, not items)
#define HID_USAGE_PAGE_GENERIC_DESKTOP  0x01
#define HID_USAGE_PAGE_KEYBOARD         0x07
#define HID_USAGE_PAGE_BUTTON           0x09

// HID Usages (values, not items)
#define HID_USAGE_KEYBOARD              0x06
#define HID_USAGE_MOUSE                 0x02
#define HID_USAGE_POINTER               0x01

uint32_t cdc_task(void);
void hid_task(void);
void send_keyboard_report(uint8_t modifier, uint8_t keycode);
void send_mouse_report(int8_t delta_x, int8_t delta_y, uint8_t buttons);
void parse_keyboard_report(uint8_t dev_addr, uint8_t instance, const uint8_t* report, uint16_t len);
void parse_mouse_report(uint8_t dev_addr, uint8_t instance, const uint8_t* report, uint16_t len);
void parse_hid_descriptor(uint8_t dev_addr, uint8_t instance, const uint8_t* desc, uint16_t desc_len);

// Non-blocking keyboard test state
uint32_t keyboard_test_deadline = 0;
bool keyboard_test_pending = false;

// Global variables for display updates
static char last_event[32] = "None";
static uint8_t last_key = 0;

// Debug control
static bool debug_raw_reports = false;


int main() {
    // Give everything time to power up properly.
    sleep_ms(2000);

    // Turn off NeoPixel IMMEDIATELY - before anything else
    gpio_init(21);  // NeoPixel data pin
    gpio_set_dir(21, GPIO_OUT);
    gpio_put(21, 0);  // Set data pin low
    gpio_init(20);  // NeoPixel power pin
    gpio_set_dir(20, GPIO_OUT);
    gpio_put(20, 0);  // Turn off power to NeoPixel

    // Initialize board
    board_init();

    // Initialize I2C for STEMMA QT connector (GPIO 2=SDA, 3=SCL)
    sleep_ms(500);
    i2c_init(i2c1, 400000);
    gpio_set_function(2, GPIO_FUNC_I2C);
    gpio_set_function(3, GPIO_FUNC_I2C);
    gpio_pull_up(2);
    gpio_pull_up(3);

    // Initialize display (optional - may not be present)
    sh1107_t display;
    bool display_ok = sh1107_init(&display, i2c1);

    // Show initial startup message
    if (display_ok) {
        sh1107_clear(&display);
        sh1107_draw_string(&display, 1, 10, "STARTING...");
        sh1107_display(&display);
        sleep_ms(500);
    }


    // Initialize Boot button (GPIO 23 on RP2040)
    gpio_init(23);
    gpio_set_dir(23, GPIO_IN);
    gpio_pull_up(23);  // Boot button pulls to ground when pressed

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
        sh1107_draw_string(&display, 1, 10, "USB DUAL MODE");
        sh1107_draw_string(&display, 1, 25, "DEVICE + HOST");
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
        byte_count += cdc_task();

        // HID host task for handling connected devices
        hid_task();


        // Check for pending keyboard test
        if (keyboard_test_pending && time_us_32() >= keyboard_test_deadline) {
            tud_cdc_write_str("PRESSING 'B'...\r\n");
            tud_cdc_write_flush();
            send_keyboard_report(0, HID_KEY_B);  // Press 'b'
            sleep_ms(100);  // Longer press duration
            tud_cdc_write_str("RELEASING 'B'...\r\n");
            tud_cdc_write_flush();
            send_keyboard_report(0, 0);          // Release 'b'
            sleep_ms(100);  // Ensure release is processed
            tud_cdc_write_str("KEY 'B' COMPLETE\r\n");
            tud_cdc_write_flush();
            keyboard_test_pending = false;
        }

        // Check Boot button (GPIO 23, active low)
        static bool boot_button_last = true;
        bool boot_button_now = gpio_get(23);
        if (boot_button_last && !boot_button_now) {  // Button press (falling edge)
            tud_cdc_write_str("BOOT BUTTON PRESSED!\r\n");
            tud_cdc_write_flush();
        }
        boot_button_last = boot_button_now;

        // Update display periodically
        static uint32_t last_display_update = 0;
        if (time_us_32() - last_display_update > 500000) { // Update every 500ms
            last_display_update = time_us_32();

            if (display_ok) {
                sh1107_clear(&display);

                // Count connected devices
                int device_count = 0;
                int kbd_count = 0;
                int mouse_count = 0;
                for (int i = 0; i < MAX_HID_DEVICES; i++) {
                    if (hid_devices[i].is_connected) {
                        device_count++;
                        if (hid_devices[i].has_keyboard) kbd_count++;
                        if (hid_devices[i].has_mouse) mouse_count++;
                    }
                }

                // Line 1: Device counts
                snprintf(status_buf, sizeof(status_buf), "HID: %d (K:%d M:%d)", device_count, kbd_count, mouse_count);
                sh1107_draw_string(&display, 1, 10, status_buf);

                // Line 2: Last event
                sh1107_draw_string(&display, 1, 25, last_event);

                // Line 3: CDC bytes for testing
                snprintf(status_buf, sizeof(status_buf), "CDC: %lu B", byte_count);
                sh1107_draw_string(&display, 1, 40, status_buf);

                sh1107_display(&display);
            }
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
                    // Schedule 'b' key to be sent in 3 seconds
                    keyboard_test_deadline = time_us_32() + 3000000; // 3 seconds in microseconds
                    keyboard_test_pending = true;
                    tud_cdc_write_str("WILL SEND 'B' IN 3 SECONDS...\r\n");
                    break;
                case 'c':
                    // Emergency: clear all keys
                    send_keyboard_report(0, 0);  // Release everything
                    sleep_ms(10);
                    send_keyboard_report(0, 0);  // Send again to be sure
                    tud_cdc_write_str("CLEARED ALL KEYS\r\n");
                    break;
                case 'D':
                    // Toggle debug raw reports
                    debug_raw_reports = !debug_raw_reports;
                    if (debug_raw_reports) {
                        tud_cdc_write_str("DEBUG: Raw report dumping ON\r\n");
                    } else {
                        tud_cdc_write_str("DEBUG: Raw report dumping OFF\r\n");
                    }
                    break;
                case 'S':
                    // Show descriptor
                    for (int i = 0; i < MAX_HID_DEVICES; i++) {
                        if (hid_devices[i].is_connected) {
                            char msg[64];
                            snprintf(msg, sizeof(msg), "DEVICE %d: addr=%d inst=%d len=%d\r\n",
                                    i, hid_devices[i].dev_addr, hid_devices[i].instance, hid_devices[i].report_desc_len);
                            tud_cdc_write_str(msg);
                        }
                    }
                    break;
                case 'H':
                    // Help
                    tud_cdc_write_str("COMMANDS:\r\n");
                    tud_cdc_write_str("  l/r/u/d - test mouse movement\r\n");
                    tud_cdc_write_str("  a - test keyboard (sends 'b')\r\n");
                    tud_cdc_write_str("  c - clear stuck keys\r\n");
                    tud_cdc_write_str("  D - toggle raw HID report debugging\r\n");
                    tud_cdc_write_str("  S - show connected devices\r\n");
                    tud_cdc_write_str("  H - show this help\r\n");
                    break;
                default:
                    // Send back normal response with the actual character
                    char response[32];
                    if (c >= 32 && c <= 126) {
                        snprintf(response, sizeof(response), "GOT '%c' (0x%02X)\r\n", c, c);
                    } else {
                        snprintf(response, sizeof(response), "GOT 0x%02X\r\n", c);
                    }
                    tud_cdc_write_str(response);
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
    // Find empty slot to store device info
    int device_slot = -1;
    for (int i = 0; i < MAX_HID_DEVICES; i++) {
        if (!hid_devices[i].is_connected) {
            hid_devices[i].dev_addr = dev_addr;
            hid_devices[i].instance = instance;
            hid_devices[i].itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);
            hid_devices[i].is_connected = true;
            hid_devices[i].report_desc_len = desc_len;
            hid_devices[i].has_keyboard = false;
            hid_devices[i].has_mouse = false;
            hid_devices[i].input_report_size = 0;
            device_slot = i;
            break;
        }
    }

    // Simple heuristic approach: assume devices can do both keyboard and mouse
    if (device_slot >= 0) {
        uint8_t itf_protocol = hid_devices[device_slot].itf_protocol;

        if (itf_protocol == HID_ITF_PROTOCOL_KEYBOARD) {
            // Standard keyboard
            hid_devices[device_slot].has_keyboard = true;
        } else if (itf_protocol == HID_ITF_PROTOCOL_MOUSE) {
            // Standard mouse
            hid_devices[device_slot].has_mouse = true;
        } else {
            // Unknown protocol - assume it's a composite device like trackpoint
            // Most trackpoint keyboards can do both
            hid_devices[device_slot].has_keyboard = true;
            hid_devices[device_slot].has_mouse = true;
        }
    }

    // Request to receive report
    tuh_hid_receive_report(dev_addr, instance);

}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
    // Clear device info quietly
    for (int i = 0; i < MAX_HID_DEVICES; i++) {
        if (hid_devices[i].is_connected &&
            hid_devices[i].dev_addr == dev_addr &&
            hid_devices[i].instance == instance) {
            hid_devices[i].is_connected = false;
            break;
        }
    }
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
    // Debug: dump raw reports if enabled
    if (debug_raw_reports) {
        char debug_msg[128];
        int offset = snprintf(debug_msg, sizeof(debug_msg), "RAW [%d,%d] len=%d: ", dev_addr, instance, len);

        for (uint16_t i = 0; i < len && offset < sizeof(debug_msg) - 4; i++) {
            offset += snprintf(debug_msg + offset, sizeof(debug_msg) - offset, "%02X ", report[i]);
        }
        offset += snprintf(debug_msg + offset, sizeof(debug_msg) - offset, "\r\n");

        tud_cdc_write_str(debug_msg);
        tud_cdc_write_flush();
    }

    // Find the device info
    for (int i = 0; i < MAX_HID_DEVICES; i++) {
        if (hid_devices[i].is_connected &&
            hid_devices[i].dev_addr == dev_addr &&
            hid_devices[i].instance == instance) {

            // Handle keyboard and mouse reports
            if (len == 8) {
                // Keyboard report: [modifier, reserved, key1, key2, key3, key4, key5, key6]
                uint8_t key = report[2]; // First key

                if (key != 0 && key != last_key) {
                    last_key = key;
                    snprintf(last_event, sizeof(last_event), "K: %02X", key);
                }
            } else if (len == 6 && report[0] == 0x01) {
                // Trackpoint mouse report: [0x01, buttons, x, y, wheel, ?]
                uint8_t buttons = report[1];
                int8_t delta_x = (int8_t)report[2];
                int8_t delta_y = (int8_t)report[3];
                int8_t wheel = (int8_t)report[4];

                if (buttons != 0 || delta_x != 0 || delta_y != 0 || wheel != 0) {
                    snprintf(last_event, sizeof(last_event), "M: B=%d X=%d Y=%d W=%d", buttons, delta_x, delta_y, wheel);
                }
            }
            break;
        }
    }

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

//--------------------------------------------------------------------+
// HID HOST PARSING FUNCTIONS
//--------------------------------------------------------------------+

void parse_keyboard_report(uint8_t dev_addr, uint8_t instance, const uint8_t* report, uint16_t len) {
    // Standard keyboard report format: [modifier, reserved, key1, key2, key3, key4, key5, key6]
    if (len < 8) {
        tud_cdc_write_str("HOST: KEYBOARD report too short\r\n");
        return;
    }

    uint8_t modifier = report[0];
    // report[1] is reserved
    uint8_t keys[6];
    for (int i = 0; i < 6; i++) {
        keys[i] = report[i + 2];
    }

    char msg[128];
    int offset = snprintf(msg, sizeof(msg), "HOST: KEYBOARD [%d,%d] mod=0x%02X keys=",
                         dev_addr, instance, modifier);

    bool has_keys = false;
    for (int i = 0; i < 6; i++) {
        if (keys[i] != 0) {
            offset += snprintf(msg + offset, sizeof(msg) - offset, "%02X ", keys[i]);
            has_keys = true;
        }
    }

    if (!has_keys) {
        offset += snprintf(msg + offset, sizeof(msg) - offset, "NONE");
    }

    offset += snprintf(msg + offset, sizeof(msg) - offset, "\r\n");
    tud_cdc_write_str(msg);
    tud_cdc_write_flush();
}

void parse_mouse_report(uint8_t dev_addr, uint8_t instance, const uint8_t* report, uint16_t len) {
    // Standard mouse report format: [buttons, x, y, wheel, ...]
    if (len < 3) {
        tud_cdc_write_str("HOST: MOUSE report too short\r\n");
        return;
    }

    uint8_t buttons = report[0];
    int8_t delta_x = (int8_t)report[1];
    int8_t delta_y = (int8_t)report[2];
    int8_t wheel = (len > 3) ? (int8_t)report[3] : 0;

    // Only print if there's actual activity (not all zeros)
    if (buttons != 0 || delta_x != 0 || delta_y != 0 || wheel != 0) {
        char msg[128];
        snprintf(msg, sizeof(msg), "HOST: MOUSE [%d,%d] btn=0x%02X x=%d y=%d wheel=%d\r\n",
                dev_addr, instance, buttons, delta_x, delta_y, wheel);
        tud_cdc_write_str(msg);
        tud_cdc_write_flush();
    }
}

//--------------------------------------------------------------------+
// HID DESCRIPTOR PARSER
//--------------------------------------------------------------------+

void parse_hid_descriptor(uint8_t dev_addr, uint8_t instance, const uint8_t* desc, uint16_t desc_len) {
    char debug_msg[64];
    snprintf(debug_msg, sizeof(debug_msg), "HOST: parse_hid_descriptor called len=%d\r\n", desc_len);
    tud_cdc_write_str(debug_msg);
    tud_cdc_write_flush();

    // Find the device slot
    int device_slot = -1;
    for (int i = 0; i < MAX_HID_DEVICES; i++) {
        if (hid_devices[i].is_connected &&
            hid_devices[i].dev_addr == dev_addr &&
            hid_devices[i].instance == instance) {
            device_slot = i;
            break;
        }
    }

    if (device_slot == -1) return;

    tud_cdc_write_str("HOST: Parsing HID descriptor...\r\n");

    uint16_t pos = 0;
    uint8_t current_usage_page = 0;
    uint8_t current_usage = 0;
    uint8_t report_size = 0;
    uint8_t report_count = 0;

    while (pos < desc_len) {
        uint8_t item = desc[pos];
        uint8_t size = item & 0x03;
        uint8_t type = (item >> 2) & 0x03;
        uint8_t tag = (item >> 4) & 0x0F;

        pos++; // Move past the item byte

        // Extract data based on size
        uint32_t data = 0;
        for (int i = 0; i < size; i++) {
            if (pos + i < desc_len) {
                data |= (desc[pos + i] << (i * 8));
            }
        }
        pos += size;

        // Parse based on tag
        switch (item & 0xFC) { // Mask out size bits
            case HID_ITEM_USAGE_PAGE:
                current_usage_page = data;
                break;

            case HID_ITEM_USAGE:
                current_usage = data;

                // Check for keyboard or mouse usage
                if (current_usage_page == HID_USAGE_PAGE_GENERIC_DESKTOP) {
                    if (current_usage == HID_USAGE_KEYBOARD) {
                        hid_devices[device_slot].has_keyboard = true;
                        tud_cdc_write_str("HOST: Found KEYBOARD usage\r\n");
                    } else if (current_usage == HID_USAGE_MOUSE || current_usage == HID_USAGE_POINTER) {
                        hid_devices[device_slot].has_mouse = true;
                        tud_cdc_write_str("HOST: Found MOUSE usage\r\n");
                    }
                } else if (current_usage_page == HID_USAGE_PAGE_KEYBOARD) {
                    hid_devices[device_slot].has_keyboard = true;
                    tud_cdc_write_str("HOST: Found KEYBOARD usage page\r\n");
                }
                break;

            case HID_ITEM_REPORT_SIZE:
                report_size = data;
                break;

            case HID_ITEM_REPORT_COUNT:
                report_count = data;
                break;

            case HID_ITEM_INPUT:
                // This defines an input report field
                if (report_size > 0 && report_count > 0) {
                    uint8_t field_size = (report_size * report_count + 7) / 8; // Convert bits to bytes
                    if (field_size > hid_devices[device_slot].input_report_size) {
                        hid_devices[device_slot].input_report_size += field_size;
                    }
                }
                break;
        }
    }

    char msg[128];
    snprintf(msg, sizeof(msg), "HOST: Descriptor parsed - kbd=%d mouse=%d report_size=%d\r\n",
             hid_devices[device_slot].has_keyboard,
             hid_devices[device_slot].has_mouse,
             hid_devices[device_slot].input_report_size);
    tud_cdc_write_str(msg);
    tud_cdc_write_flush();
}
