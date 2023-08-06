/**
 * @file matrix_scan.c
 * @author astro
 *  keyboard matrix scanning implementation
 */

#include <string.h>

#include "matrix.h"
#include "amk_gpio.h"
#include "amk_printf.h"
#include "wait.h"

#ifndef MATRIX_SCAN_DEBUG
#define MATRIX_SCAN_DEBUG 1
#endif

#if MATRIX_SCAN_DEBUG
#define matrix_scan_debug  amk_printf
#else
#define matrix_scan_debug(...)
#endif

#ifndef DEBOUNCE
#   define DEBOUNCE 5
#endif

static pin_t col_pins[] = MATRIX_COL_PINS;
static pin_t row_pins[] = MATRIX_ROW_PINS;

void matrix_init_custom(void)
{
    for (int col = 0; col < MATRIX_COLS; col++) {
        gpio_set_output_pushpull(col_pins[col]);
        gpio_write_pin(col_pins[col], 0);
    }

    for (int row = 0; row < MATRIX_ROWS; row++) {
        gpio_set_input_pulldown(row_pins[row]);
    }
}

bool matrix_scan_custom(matrix_row_t raw[]) 
{
    bool changed = false;
    for (int col = 0; col < MATRIX_COLS; col++) {
        gpio_write_pin(col_pins[col], 1);
        wait_us(10);

        for(uint8_t row = 0; row < MATRIX_ROWS; row++) {
            matrix_row_t last_row_value    = raw[row];
            matrix_row_t current_row_value = last_row_value;

            if (gpio_read_pin(row_pins[row])) {
                current_row_value |= (1 << col);
            } else {
                current_row_value &= ~(1 << col);
            }

            if (last_row_value != current_row_value) {
                raw[row] = current_row_value;
                changed = true;
            }
        }
        gpio_write_pin(col_pins[col], 0);
    }

    if (changed) {
        for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
            matrix_scan_debug("row:%d-%x\n", row, matrix_get_row(row));
        }
    }
    return changed;
}
