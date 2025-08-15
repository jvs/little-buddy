#ifndef HID_PARSER_H
#define HID_PARSER_H

#include <stdint.h>
#include <stdbool.h>

// HID Usage Pages (we only care about these)
#define HID_USAGE_PAGE_GENERIC_DESKTOP  0x01
#define HID_USAGE_PAGE_KEYBOARD         0x07
#define HID_USAGE_PAGE_BUTTON           0x09

// HID Usages for Generic Desktop
#define HID_USAGE_POINTER               0x01
#define HID_USAGE_MOUSE                 0x02
#define HID_USAGE_KEYBOARD              0x06
#define HID_USAGE_X                     0x30
#define HID_USAGE_Y                     0x31
#define HID_USAGE_WHEEL                 0x38

// HID Item constants
#define HID_INPUT                       0x80
#define HID_OUTPUT                      0x90
#define HID_FEATURE                     0xB0
#define HID_COLLECTION                  0xA0
#define HID_END_COLLECTION              0xC0
#define HID_USAGE_PAGE                  0x04
#define HID_USAGE                       0x08
#define HID_USAGE_MINIMUM               0x18
#define HID_USAGE_MAXIMUM               0x28
#define HID_LOGICAL_MINIMUM             0x14
#define HID_LOGICAL_MAXIMUM             0x24
#define HID_REPORT_SIZE                 0x74
#define HID_REPORT_ID                   0x84
#define HID_REPORT_COUNT                0x94

// Simplified usage definition
typedef struct {
    uint8_t report_id;
    uint8_t bit_pos;
    uint8_t bit_size;
    uint16_t usage_page;
    uint16_t usage;
    bool is_relative;
    int32_t logical_min;
    int32_t logical_max;
} hid_usage_t;

// Input types we care about
typedef enum {
    INPUT_TYPE_UNKNOWN = 0,
    INPUT_TYPE_MOUSE_X,
    INPUT_TYPE_MOUSE_Y,
    INPUT_TYPE_MOUSE_WHEEL,
    INPUT_TYPE_MOUSE_BUTTON,
    INPUT_TYPE_KEYBOARD_MODIFIER,
    INPUT_TYPE_KEYBOARD_KEY
} input_type_t;

#define MAX_USAGES 32

typedef struct {
    hid_usage_t usages[MAX_USAGES];
    uint8_t usage_count;
} hid_descriptor_t;

bool hid_parse_descriptor(const uint8_t* desc, uint16_t desc_len, hid_descriptor_t* parsed);
input_type_t hid_get_input_type(const hid_usage_t* usage);
bool hid_extract_value(const uint8_t* report, uint8_t report_len, const hid_usage_t* usage, int32_t* value);

#endif