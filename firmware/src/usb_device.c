#include "usb_device.h"
#include "usb_host.h"  // For debug control
#include <pico/stdlib.h>
#include <string.h>
#include <stdio.h>


void usb_device_init(void) {
    // Initialize TinyUSB device stack
    tud_init(BOARD_TUD_RHPORT);
}

void usb_device_task(void) {
    // TinyUSB device task
    tud_task();
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
// OUTPUT QUEUE PROCESSING
//--------------------------------------------------------------------+

void usb_device_process_output_queue(usb_output_queue_t* queue) {
    usb_output_event_t event;
    
    // Process all events in the queue
    while (usb_output_queue_dequeue(queue, &event)) {
        switch (event.type) {
            case USB_OUTPUT_MOUSE:
                send_mouse_report(
                    event.data.mouse.delta_x,
                    event.data.mouse.delta_y,
                    event.data.mouse.buttons
                );
                break;
                
            case USB_OUTPUT_KEYBOARD:
                // For keyboard, we need to handle press/release differently
                if (event.data.keyboard.pressed) {
                    send_keyboard_report(
                        event.data.keyboard.modifier,
                        event.data.keyboard.keycode
                    );
                } else {
                    // Key release - send with no keycode
                    send_keyboard_report(0, 0);
                }
                break;
                
            default:
                // Unknown event type, skip
                break;
        }
    }
}

//--------------------------------------------------------------------+
// USB DEVICE DESCRIPTORS
//--------------------------------------------------------------------+

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
