/* Copyright 2017 Jason Williams (Wilba)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "dynamic_keymap.h"
#include "keymap_introspection.h"
#include "action.h"
#include "eeprom.h"
#include "progmem.h"
#include "send_string.h"
#include "keycodes.h"
#include "action_tapping.h"
#include "wait.h"
#include <string.h>

#ifdef VIA_ENABLE
#    include "via.h"
#    define DYNAMIC_KEYMAP_EEPROM_START (VIA_EEPROM_CONFIG_END)
#else
#    include "eeconfig.h"
#    define DYNAMIC_KEYMAP_EEPROM_START (EECONFIG_SIZE)
#endif

#ifdef VIAL_ENABLE
#include "vial.h"
#endif

#ifdef ENCODER_ENABLE
#    include "encoder.h"
#else
#    define NUM_ENCODERS 0
#endif

#ifndef TOTAL_EEPROM_BYTE_COUNT
#    error Unknown total EEPROM size. Cannot derive maximum for dynamic keymaps.
#endif

#ifndef DYNAMIC_KEYMAP_EEPROM_MAX_ADDR
#    define DYNAMIC_KEYMAP_EEPROM_MAX_ADDR (TOTAL_EEPROM_BYTE_COUNT - 1)
#endif

#if DYNAMIC_KEYMAP_EEPROM_MAX_ADDR > (TOTAL_EEPROM_BYTE_COUNT - 1)
#    pragma message STR(DYNAMIC_KEYMAP_EEPROM_MAX_ADDR) " > " STR((TOTAL_EEPROM_BYTE_COUNT - 1))
#    error DYNAMIC_KEYMAP_EEPROM_MAX_ADDR is configured to use more space than what is available for the selected EEPROM driver
#endif

// Due to usage of uint16_t check for max 65535
#if DYNAMIC_KEYMAP_EEPROM_MAX_ADDR > 65535
#    pragma message STR(DYNAMIC_KEYMAP_EEPROM_MAX_ADDR) " > 65535"
#    error DYNAMIC_KEYMAP_EEPROM_MAX_ADDR must be less than 65536
#endif

// If DYNAMIC_KEYMAP_EEPROM_ADDR not explicitly defined in config.h,
#ifndef DYNAMIC_KEYMAP_EEPROM_ADDR
#    define DYNAMIC_KEYMAP_EEPROM_ADDR DYNAMIC_KEYMAP_EEPROM_START
#endif

// Encoders are located right after the dynamic keymap
#define VIAL_ENCODERS_EEPROM_ADDR (DYNAMIC_KEYMAP_EEPROM_ADDR + (DYNAMIC_KEYMAP_LAYER_COUNT * MATRIX_ROWS * MATRIX_COLS * 2))
#define DYNAMIC_KEYMAP_ENCODER_EEPROM_ADDR VIAL_ENCODERS_EEPROM_ADDR

#define VIAL_ENCODERS_SIZE (NUM_ENCODERS * DYNAMIC_KEYMAP_LAYER_COUNT * 2 * 2)

// QMK settings area is just past encoders
#define VIAL_QMK_SETTINGS_EEPROM_ADDR (VIAL_ENCODERS_EEPROM_ADDR + VIAL_ENCODERS_SIZE)

#ifdef QMK_SETTINGS
#include "qmk_settings.h"
#define VIAL_QMK_SETTINGS_SIZE (sizeof(qmk_settings_t))
#else
#define VIAL_QMK_SETTINGS_SIZE 0
#endif

// Tap-dance
#define VIAL_TAP_DANCE_EEPROM_ADDR (VIAL_QMK_SETTINGS_EEPROM_ADDR + VIAL_QMK_SETTINGS_SIZE)

#ifdef VIAL_TAP_DANCE_ENABLE
#define VIAL_TAP_DANCE_SIZE (sizeof(vial_tap_dance_entry_t) * VIAL_TAP_DANCE_ENTRIES)
#else
#define VIAL_TAP_DANCE_SIZE 0
#endif

// Combos
#define VIAL_COMBO_EEPROM_ADDR (VIAL_TAP_DANCE_EEPROM_ADDR + VIAL_TAP_DANCE_SIZE)

#ifdef VIAL_COMBO_ENABLE
#define VIAL_COMBO_SIZE (sizeof(vial_combo_entry_t) * VIAL_COMBO_ENTRIES)
#else
#define VIAL_COMBO_SIZE 0
#endif

// Key overrides
#define VIAL_KEY_OVERRIDE_EEPROM_ADDR (VIAL_COMBO_EEPROM_ADDR + VIAL_COMBO_SIZE)

#ifdef VIAL_KEY_OVERRIDE_ENABLE
#define VIAL_KEY_OVERRIDE_SIZE (sizeof(vial_key_override_entry_t) * VIAL_KEY_OVERRIDE_ENTRIES)
#else
#define VIAL_KEY_OVERRIDE_SIZE 0
#endif

// Dynamic macro
#ifndef DYNAMIC_KEYMAP_MACRO_EEPROM_ADDR
#    define DYNAMIC_KEYMAP_MACRO_EEPROM_ADDR (VIAL_KEY_OVERRIDE_EEPROM_ADDR + VIAL_KEY_OVERRIDE_SIZE)
#endif

// Sanity check that dynamic keymaps fit in available EEPROM
// If there's not 100 bytes available for macros, then something is wrong.
// The keyboard should override DYNAMIC_KEYMAP_LAYER_COUNT to reduce it,
// or DYNAMIC_KEYMAP_EEPROM_MAX_ADDR to increase it, *only if* the microcontroller has
// more than the default.
_Static_assert(DYNAMIC_KEYMAP_EEPROM_MAX_ADDR >= DYNAMIC_KEYMAP_MACRO_EEPROM_ADDR + 100, "Dynamic keymaps are configured to use more EEPROM than is available.");

// Dynamic macros are stored after the keymaps and use what is available
// up to and including DYNAMIC_KEYMAP_EEPROM_MAX_ADDR.
#ifndef DYNAMIC_KEYMAP_MACRO_EEPROM_SIZE
#    define DYNAMIC_KEYMAP_MACRO_EEPROM_SIZE (DYNAMIC_KEYMAP_EEPROM_MAX_ADDR - DYNAMIC_KEYMAP_MACRO_EEPROM_ADDR + 1)
#endif

#ifndef DYNAMIC_KEYMAP_MACRO_DELAY
#    define DYNAMIC_KEYMAP_MACRO_DELAY TAP_CODE_DELAY
#endif

uint8_t dynamic_keymap_get_layer_count(void) {
    return DYNAMIC_KEYMAP_LAYER_COUNT;
}

void *dynamic_keymap_key_to_eeprom_address(uint8_t layer, uint8_t row, uint8_t column) {
    // TODO: optimize this with some left shifts
    return ((void *)DYNAMIC_KEYMAP_EEPROM_ADDR) + (layer * MATRIX_ROWS * MATRIX_COLS * 2) + (row * MATRIX_COLS * 2) + (column * 2);
}

uint16_t dynamic_keymap_get_keycode(uint8_t layer, uint8_t row, uint8_t column) {
    if (layer >= DYNAMIC_KEYMAP_LAYER_COUNT || row >= MATRIX_ROWS || column >= MATRIX_COLS) return KC_NO;
    void *address = dynamic_keymap_key_to_eeprom_address(layer, row, column);
    // Big endian, so we can read/write EEPROM directly from host if we want
    uint16_t keycode = eeprom_read_byte(address) << 8;
    keycode |= eeprom_read_byte(address + 1);
    return keycode;
}

void dynamic_keymap_set_keycode(uint8_t layer, uint8_t row, uint8_t column, uint16_t keycode) {
    if (layer >= DYNAMIC_KEYMAP_LAYER_COUNT || row >= MATRIX_ROWS || column >= MATRIX_COLS) return;
    void *address = dynamic_keymap_key_to_eeprom_address(layer, row, column);
    // Big endian, so we can read/write EEPROM directly from host if we want
    eeprom_update_byte(address, (uint8_t)(keycode >> 8));
    eeprom_update_byte(address + 1, (uint8_t)(keycode & 0xFF));
}

#ifdef ENCODER_MAP_ENABLE
void *dynamic_keymap_encoder_to_eeprom_address(uint8_t layer, uint8_t encoder_id) {
    return ((void *)DYNAMIC_KEYMAP_ENCODER_EEPROM_ADDR) + (layer * NUM_ENCODERS * 2 * 2) + (encoder_id * 2 * 2);
}

uint16_t dynamic_keymap_get_encoder(uint8_t layer, uint8_t encoder_id, bool clockwise) {
    if (layer >= DYNAMIC_KEYMAP_LAYER_COUNT || encoder_id >= NUM_ENCODERS) return KC_NO;
    void *address = dynamic_keymap_encoder_to_eeprom_address(layer, encoder_id);
    // Big endian, so we can read/write EEPROM directly from host if we want
    uint16_t keycode = ((uint16_t)eeprom_read_byte(address + (clockwise ? 0 : 2))) << 8;
    keycode |= eeprom_read_byte(address + (clockwise ? 0 : 2) + 1);
    return keycode;
}

void dynamic_keymap_set_encoder(uint8_t layer, uint8_t encoder_id, bool clockwise, uint16_t keycode) {
    if (layer >= DYNAMIC_KEYMAP_LAYER_COUNT || encoder_id >= NUM_ENCODERS) return;
    void *address = dynamic_keymap_encoder_to_eeprom_address(layer, encoder_id);
    // Big endian, so we can read/write EEPROM directly from host if we want
    eeprom_update_byte(address + (clockwise ? 0 : 2), (uint8_t)(keycode >> 8));
    eeprom_update_byte(address + (clockwise ? 0 : 2) + 1, (uint8_t)(keycode & 0xFF));
}
#endif // ENCODER_MAP_ENABLE

#ifdef QMK_SETTINGS
uint8_t dynamic_keymap_get_qmk_settings(uint16_t offset) {
    if (offset >= VIAL_QMK_SETTINGS_SIZE)
        return 0;

    void *address = (void*)(VIAL_QMK_SETTINGS_EEPROM_ADDR + offset);
    return eeprom_read_byte(address);
}

void dynamic_keymap_set_qmk_settings(uint16_t offset, uint8_t value) {
    if (offset >= VIAL_QMK_SETTINGS_SIZE)
        return;

    void *address = (void*)(VIAL_QMK_SETTINGS_EEPROM_ADDR + offset);
    eeprom_update_byte(address, value);
}
#endif

#ifdef VIAL_TAP_DANCE_ENABLE
int dynamic_keymap_get_tap_dance(uint8_t index, vial_tap_dance_entry_t *entry) {
    if (index >= VIAL_TAP_DANCE_ENTRIES)
        return -1;

    void *address = (void*)(VIAL_TAP_DANCE_EEPROM_ADDR + index * sizeof(vial_tap_dance_entry_t));
    eeprom_read_block(entry, address, sizeof(vial_tap_dance_entry_t));

    return 0;
}

int dynamic_keymap_set_tap_dance(uint8_t index, const vial_tap_dance_entry_t *entry) {
    if (index >= VIAL_TAP_DANCE_ENTRIES)
        return -1;

    void *address = (void*)(VIAL_TAP_DANCE_EEPROM_ADDR + index * sizeof(vial_tap_dance_entry_t));
    eeprom_write_block(entry, address, sizeof(vial_tap_dance_entry_t));

    return 0;
}
#endif

#ifdef VIAL_COMBO_ENABLE
int dynamic_keymap_get_combo(uint8_t index, vial_combo_entry_t *entry) {
    if (index >= VIAL_COMBO_ENTRIES)
        return -1;

    void *address = (void*)(VIAL_COMBO_EEPROM_ADDR + index * sizeof(vial_combo_entry_t));
    eeprom_read_block(entry, address, sizeof(vial_combo_entry_t));

    return 0;
}

int dynamic_keymap_set_combo(uint8_t index, const vial_combo_entry_t *entry) {
    if (index >= VIAL_COMBO_ENTRIES)
        return -1;

    void *address = (void*)(VIAL_COMBO_EEPROM_ADDR + index * sizeof(vial_combo_entry_t));
    eeprom_write_block(entry, address, sizeof(vial_combo_entry_t));

    return 0;
}
#endif

#ifdef VIAL_KEY_OVERRIDE_ENABLE
int dynamic_keymap_get_key_override(uint8_t index, vial_key_override_entry_t *entry) {
    if (index >= VIAL_KEY_OVERRIDE_ENTRIES)
        return -1;

    void *address = (void*)(VIAL_KEY_OVERRIDE_EEPROM_ADDR + index * sizeof(vial_key_override_entry_t));
    eeprom_read_block(entry, address, sizeof(vial_key_override_entry_t));

    return 0;
}

int dynamic_keymap_set_key_override(uint8_t index, const vial_key_override_entry_t *entry) {
    if (index >= VIAL_KEY_OVERRIDE_ENTRIES)
        return -1;

    void *address = (void*)(VIAL_KEY_OVERRIDE_EEPROM_ADDR + index * sizeof(vial_key_override_entry_t));
    eeprom_write_block(entry, address, sizeof(vial_key_override_entry_t));

    return 0;
}
#endif

void dynamic_keymap_reset(void) {
#ifdef VIAL_ENABLE
    /* temporarily unlock the keyboard so we can set hardcoded QK_BOOT keycode */
    int vial_unlocked_prev = vial_unlocked;
    vial_unlocked = 1;
#endif

    // Reset the keymaps in EEPROM to what is in flash.
    for (int layer = 0; layer < DYNAMIC_KEYMAP_LAYER_COUNT; layer++) {
        for (int row = 0; row < MATRIX_ROWS; row++) {
            for (int column = 0; column < MATRIX_COLS; column++) {
                dynamic_keymap_set_keycode(layer, row, column, keycode_at_keymap_location_raw(layer, row, column));
            }
        }
#ifdef ENCODER_MAP_ENABLE
        for (int encoder = 0; encoder < NUM_ENCODERS; encoder++) {
            dynamic_keymap_set_encoder(layer, encoder, true, keycode_at_encodermap_location_raw(layer, encoder, true));
            dynamic_keymap_set_encoder(layer, encoder, false, keycode_at_encodermap_location_raw(layer, encoder, false));
        }
#endif // ENCODER_MAP_ENABLE
    }

#ifdef QMK_SETTINGS
    qmk_settings_reset();
#endif

#ifdef VIAL_TAP_DANCE_ENABLE
    vial_tap_dance_entry_t td = { KC_NO, KC_NO, KC_NO, KC_NO, TAPPING_TERM };
    for (size_t i = 0; i < VIAL_TAP_DANCE_ENTRIES; ++i) {
        dynamic_keymap_set_tap_dance(i, &td);
    }
#endif

#ifdef VIAL_COMBO_ENABLE
    vial_combo_entry_t combo = { 0 };
    for (size_t i = 0; i < VIAL_COMBO_ENTRIES; ++i)
        dynamic_keymap_set_combo(i, &combo);
#endif

#ifdef VIAL_KEY_OVERRIDE_ENABLE
    vial_key_override_entry_t ko = { 0 };
    ko.layers = ~0;
    ko.options = vial_ko_option_activation_negative_mod_up | vial_ko_option_activation_required_mod_down | vial_ko_option_activation_trigger_down;
    for (size_t i = 0; i < VIAL_KEY_OVERRIDE_ENTRIES; ++i)
        dynamic_keymap_set_key_override(i, &ko);
#endif

#ifdef VIAL_ENABLE
    /* re-lock the keyboard */
    vial_unlocked = vial_unlocked_prev;
#endif
}

void dynamic_keymap_get_buffer(uint16_t offset, uint16_t size, uint8_t *data) {
    uint16_t dynamic_keymap_eeprom_size = DYNAMIC_KEYMAP_LAYER_COUNT * MATRIX_ROWS * MATRIX_COLS * 2;
    void *   source                     = (void *)(DYNAMIC_KEYMAP_EEPROM_ADDR + offset);
    uint8_t *target                     = data;
    for (uint16_t i = 0; i < size; i++) {
        if (offset + i < dynamic_keymap_eeprom_size) {
            *target = eeprom_read_byte(source);
        } else {
            *target = 0x00;
        }
        source++;
        target++;
    }
}

void dynamic_keymap_set_buffer(uint16_t offset, uint16_t size, uint8_t *data) {
    uint16_t dynamic_keymap_eeprom_size = DYNAMIC_KEYMAP_LAYER_COUNT * MATRIX_ROWS * MATRIX_COLS * 2;
    void *   target                     = (void *)(DYNAMIC_KEYMAP_EEPROM_ADDR + offset);
    uint8_t *source                     = data;

#ifdef VIAL_ENABLE
    /* ensure the writes are bounded */
    if (offset >= dynamic_keymap_eeprom_size || dynamic_keymap_eeprom_size - offset < size)
        return;

#ifndef VIAL_INSECURE
    /* Check whether it is trying to send a QK_BOOT keycode; only allow setting these if unlocked */
    if (!vial_unlocked) {
        /* how much of the input array we'll have to check in the loop */
        uint16_t chk_offset = 0;
        uint16_t chk_sz = size;

        /* initial byte misaligned -- this means the first keycode will be a combination of existing and new data */
        if (offset % 2 != 0) {
            uint16_t kc = (eeprom_read_byte((uint8_t*)target - 1) << 8) | data[0];
            if (kc == QK_BOOT)
                data[0] = 0xFF;

            /* no longer have to check the first byte */
            chk_offset += 1;
        }

        /* final byte misaligned -- this means the last keycode will be a combination of new and existing data */
        if ((offset + size) % 2 != 0) {
            uint16_t kc = (data[size - 1] << 8) | eeprom_read_byte((uint8_t*)target + size);
            if (kc == QK_BOOT)
                data[size - 1] = 0xFF;

            /* no longer have to check the last byte */
            chk_sz -= 1;
        }

        /* check the entire array, replace any instances of QK_BOOT with invalid keycode 0xFFFF */
        for (uint16_t i = chk_offset; i < chk_sz; i += 2) {
            uint16_t kc = (data[i] << 8) | data[i + 1];
            if (kc == QK_BOOT) {
                data[i] = 0xFF;
                data[i + 1] = 0xFF;
            }
        }
    }
#endif
#endif

    for (uint16_t i = 0; i < size; i++) {
        if (offset + i < dynamic_keymap_eeprom_size) {
            eeprom_update_byte(target, *source);
        }
        source++;
        target++;
    }
}

uint16_t keycode_at_keymap_location(uint8_t layer_num, uint8_t row, uint8_t column) {
    if (layer_num < DYNAMIC_KEYMAP_LAYER_COUNT && row < MATRIX_ROWS && column < MATRIX_COLS) {
        return dynamic_keymap_get_keycode(layer_num, row, column);
    }
    return KC_NO;
}

#ifdef ENCODER_MAP_ENABLE
uint16_t keycode_at_encodermap_location(uint8_t layer_num, uint8_t encoder_idx, bool clockwise) {
    if (layer_num < DYNAMIC_KEYMAP_LAYER_COUNT && encoder_idx < NUM_ENCODERS) {
        return dynamic_keymap_get_encoder(layer_num, encoder_idx, clockwise);
    }
    return KC_NO;
}
#endif // ENCODER_MAP_ENABLE

uint8_t dynamic_keymap_macro_get_count(void) {
    return DYNAMIC_KEYMAP_MACRO_COUNT;
}

uint16_t dynamic_keymap_macro_get_buffer_size(void) {
    return DYNAMIC_KEYMAP_MACRO_EEPROM_SIZE;
}

void dynamic_keymap_macro_get_buffer(uint16_t offset, uint16_t size, uint8_t *data) {
    void *   source = (void *)(DYNAMIC_KEYMAP_MACRO_EEPROM_ADDR + offset);
    uint8_t *target = data;
    for (uint16_t i = 0; i < size; i++) {
        if (offset + i < DYNAMIC_KEYMAP_MACRO_EEPROM_SIZE) {
            *target = eeprom_read_byte(source);
        } else {
            *target = 0x00;
        }
        source++;
        target++;
    }
}

void dynamic_keymap_macro_set_buffer(uint16_t offset, uint16_t size, uint8_t *data) {
    void *   target = (void *)(DYNAMIC_KEYMAP_MACRO_EEPROM_ADDR + offset);
    uint8_t *source = data;
    for (uint16_t i = 0; i < size; i++) {
        if (offset + i < DYNAMIC_KEYMAP_MACRO_EEPROM_SIZE) {
            eeprom_update_byte(target, *source);
        }
        source++;
        target++;
    }
}

void dynamic_keymap_macro_reset(void) {
    void *p   = (void *)(DYNAMIC_KEYMAP_MACRO_EEPROM_ADDR);
    void *end = (void *)(DYNAMIC_KEYMAP_MACRO_EEPROM_ADDR + DYNAMIC_KEYMAP_MACRO_EEPROM_SIZE);
    while (p != end) {
        eeprom_update_byte(p, 0);
        ++p;
    }
}

static uint16_t decode_keycode(uint16_t kc) {
    /* map 0xFF01 => 0x0100; 0xFF02 => 0x0200, etc */
    if (kc > 0xFF00)
        return (kc & 0xFF) << 8;
    return kc;
}

#include "usb_common.h"
#include "usb_interface.h"

void dynamic_keymap_macro_send(uint8_t id) {
    if (id >= DYNAMIC_KEYMAP_MACRO_COUNT) {
        return;
    }

    // Check the last byte of the buffer.
    // If it's not zero, then we are in the middle
    // of buffer writing, possibly an aborted buffer
    // write. So do nothing.
    void *p = (void *)(DYNAMIC_KEYMAP_MACRO_EEPROM_ADDR + DYNAMIC_KEYMAP_MACRO_EEPROM_SIZE - 1);
    if (eeprom_read_byte(p) != 0) {
        return;
    }

    // Skip N null characters
    // p will then point to the Nth macro
    p         = (void *)(DYNAMIC_KEYMAP_MACRO_EEPROM_ADDR);
    void *end = (void *)(DYNAMIC_KEYMAP_MACRO_EEPROM_ADDR + DYNAMIC_KEYMAP_MACRO_EEPROM_SIZE);
    while (id > 0) {
        // If we are past the end of the buffer, then the buffer
        // contents are garbage, i.e. there were not DYNAMIC_KEYMAP_MACRO_COUNT
        // nulls in the buffer.
        if (p == end) {
            return;
        }
        if (eeprom_read_byte(p) == 0) {
            --id;
        }
        ++p;
    }

    // Send the macro string one or three chars at a time
    // by making temporary 1 or 3 char strings
    char data[4] = {0, 0, 0, 0};
    // We already checked there was a null at the end of
    // the buffer, so this cannot go past the end
    while (1) {
        data[0] = eeprom_read_byte(p++);
        data[1] = 0;
        // Stop at the null terminator of this macro string
        if (data[0] == 0) {
            break;
        }
        if (data[0] == SS_QMK_PREFIX) {
            // If the char is magic, process it as indicated by the next character
            // (tap, down, up, delay)
            data[1] = eeprom_read_byte(p++);
            if (data[1] == 0)
                break;
            if (data[1] == SS_TAP_CODE || data[1] == SS_DOWN_CODE || data[1] == SS_UP_CODE) {
                // For tap, down, up, just stuff it into the array and send_string it
                data[2] = eeprom_read_byte(p++);
                if (data[2] != 0)
                    send_string(data);
            } else if (data[1] == VIAL_MACRO_EXT_TAP || data[1] == VIAL_MACRO_EXT_DOWN || data[1] == VIAL_MACRO_EXT_UP) {
                data[2] = eeprom_read_byte(p++);
                if (data[2] != 0) {
                    data[3] = eeprom_read_byte(p++);
                    if (data[3] != 0) {
                        uint16_t kc;
                        memcpy(&kc, &data[2], sizeof(kc));
                        kc = decode_keycode(kc);
                        switch (data[1]) {
                        case VIAL_MACRO_EXT_TAP:
                            vial_keycode_tap(kc);
                            break;
                        case VIAL_MACRO_EXT_DOWN:
                            vial_keycode_down(kc);
                            break;
                        case VIAL_MACRO_EXT_UP:
                            vial_keycode_up(kc);
                            break;
                        }
                    }
                }
            } else if (data[1] == SS_DELAY_CODE) {
                // For delay, decode the delay and wait_ms for that amount
                uint8_t d0 = eeprom_read_byte(p++);
                uint8_t d1 = eeprom_read_byte(p++);
                if (d0 == 0 || d1 == 0)
                    break;
                // we cannot use 0 for these, need to subtract 1 and use 255 instead of 256 for delay calculation
                int ms = (d0 - 1) + (d1 - 1) * 255;
                //while (ms--) wait_ms(1);

                uint16_t delay = (uint16_t)ms;
                #if defined(NRF52) || defined(NRF52840_XXAA)
                nrf_usb_send_report(NRF_REPORT_ID_DELAY, &delay, sizeof(delay));
                #else
                usb_send_report(HID_REPORT_ID_DELAY, &delay, sizeof(delay));
                #endif
            }
        } else {
            // If the char wasn't magic, just send it
            send_string_with_delay(data, DYNAMIC_KEYMAP_MACRO_DELAY);
        }
    }
}
