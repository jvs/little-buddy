#include "hid_parser.h"
#include <string.h>

// Helper function to get item size from HID item header
static uint8_t get_item_size(uint8_t item) {
    switch (item & 0x03) {
        case 0: return 0;
        case 1: return 1;
        case 2: return 2;
        case 3: return 4;
    }
    return 0;
}

// Helper function to extract value from HID item data
static int32_t get_item_value(const uint8_t* data, uint8_t size) {
    switch (size) {
        case 0: return 0;
        case 1: return (int8_t)data[0];
        case 2: return (int16_t)(data[0] | (data[1] << 8));
        case 4: return (int32_t)(data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24));
    }
    return 0;
}

bool hid_parse_descriptor(const uint8_t* desc, uint16_t desc_len, hid_descriptor_t* parsed) {
    if (!desc || !parsed || desc_len == 0) {
        return false;
    }
    
    memset(parsed, 0, sizeof(hid_descriptor_t));
    
    // Parsing state
    uint16_t usage_page = 0;
    uint16_t usage = 0;
    uint8_t report_id = 0;
    uint8_t report_size = 0;
    uint8_t report_count = 0;
    int32_t logical_min = 0;
    int32_t logical_max = 0;
    uint8_t bit_pos = 0;
    bool has_report_id = false;
    
    uint16_t pos = 0;
    while (pos < desc_len && parsed->usage_count < MAX_USAGES) {
        uint8_t item = desc[pos];
        uint8_t size = get_item_size(item);
        
        if (pos + size >= desc_len) break;
        
        int32_t value = get_item_value(&desc[pos + 1], size);
        
        switch (item & 0xFC) {
            case HID_USAGE_PAGE:
                usage_page = value;
                break;
                
            case HID_USAGE:
                usage = value;
                break;
                
            case HID_REPORT_ID:
                report_id = value;
                has_report_id = true;
                bit_pos = has_report_id ? 8 : 0; // Account for report ID byte
                break;
                
            case HID_REPORT_SIZE:
                report_size = value;
                break;
                
            case HID_REPORT_COUNT:
                report_count = value;
                break;
                
            case HID_LOGICAL_MINIMUM:
                logical_min = value;
                break;
                
            case HID_LOGICAL_MAXIMUM:
                logical_max = value;
                break;
                
            case HID_INPUT:
                // Only process input items, ignore constant bits (bit 0 set)
                if (!(value & 0x01)) {
                    // Check if this is a usage we care about
                    if ((usage_page == HID_USAGE_PAGE_GENERIC_DESKTOP && 
                         (usage == HID_USAGE_X || usage == HID_USAGE_Y || usage == HID_USAGE_WHEEL)) ||
                        (usage_page == HID_USAGE_PAGE_BUTTON) ||
                        (usage_page == HID_USAGE_PAGE_KEYBOARD)) {
                        
                        hid_usage_t* u = &parsed->usages[parsed->usage_count];
                        u->report_id = report_id;
                        u->bit_pos = bit_pos;
                        u->bit_size = report_size;
                        u->usage_page = usage_page;
                        u->usage = usage;
                        u->is_relative = (value & 0x04) != 0; // Bit 2 = relative
                        u->logical_min = logical_min;
                        u->logical_max = logical_max;
                        
                        parsed->usage_count++;
                    }
                }
                
                // Advance bit position for next field
                bit_pos += report_size * report_count;
                
                // Reset usage for next item
                usage = 0;
                break;
                
            case HID_COLLECTION:
                // Reset bit position at start of new collection
                bit_pos = has_report_id ? 8 : 0;
                break;
        }
        
        pos += size + 1;
    }
    
    return parsed->usage_count > 0;
}

input_type_t hid_get_input_type(const hid_usage_t* usage) {
    if (!usage) return INPUT_TYPE_UNKNOWN;
    
    switch (usage->usage_page) {
        case HID_USAGE_PAGE_GENERIC_DESKTOP:
            switch (usage->usage) {
                case HID_USAGE_X: return INPUT_TYPE_MOUSE_X;
                case HID_USAGE_Y: return INPUT_TYPE_MOUSE_Y;
                case HID_USAGE_WHEEL: return INPUT_TYPE_MOUSE_WHEEL;
            }
            break;
            
        case HID_USAGE_PAGE_BUTTON:
            return INPUT_TYPE_MOUSE_BUTTON;
            
        case HID_USAGE_PAGE_KEYBOARD:
            if (usage->usage == 0xE0) { // Left Control
                return INPUT_TYPE_KEYBOARD_MODIFIER;
            } else if (usage->usage >= 0x04 && usage->usage <= 0x65) {
                return INPUT_TYPE_KEYBOARD_KEY;
            }
            break;
    }
    
    return INPUT_TYPE_UNKNOWN;
}

bool hid_extract_value(const uint8_t* report, uint8_t report_len, const hid_usage_t* usage, int32_t* value) {
    if (!report || !usage || !value) return false;
    
    // Check if report ID matches (if report has ID)
    uint8_t offset = 0;
    if (report_len > 0 && usage->report_id != 0) {
        if (report[0] != usage->report_id) return false;
        offset = 1;
    }
    
    uint8_t byte_pos = usage->bit_pos / 8 + offset;
    uint8_t bit_offset = usage->bit_pos % 8;
    
    if (byte_pos >= report_len) return false;
    
    // Simple extraction for 1-8 bit fields
    if (usage->bit_size <= 8 && bit_offset + usage->bit_size <= 8) {
        uint8_t mask = (1 << usage->bit_size) - 1;
        *value = (report[byte_pos] >> bit_offset) & mask;
        
        // Sign extend if signed
        if (usage->logical_min < 0 && (*value & (1 << (usage->bit_size - 1)))) {
            *value |= (~0U << usage->bit_size);
        }
        
        return true;
    }
    
    // Handle 16-bit fields crossing byte boundaries
    if (usage->bit_size <= 16 && byte_pos + 1 < report_len) {
        uint16_t raw = report[byte_pos] | (report[byte_pos + 1] << 8);
        uint16_t mask = (1 << usage->bit_size) - 1;
        *value = (raw >> bit_offset) & mask;
        
        // Sign extend if signed
        if (usage->logical_min < 0 && (*value & (1 << (usage->bit_size - 1)))) {
            *value |= (~0U << usage->bit_size);
        }
        
        return true;
    }
    
    return false;
}