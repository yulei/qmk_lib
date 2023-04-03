/**
 * @file qmk_driver.c
 * @author astro
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include "qmk_driver.h"
#include "quantum/keyboard.h"

void qmk_driver_init(void)
{
    keyboard_setup();
    keyboard_init();
}

void qmk_driver_task(void)
{
    keyboard_task();
}

#include "SEGGER_RTT.h"

int8_t sendchar(uint8_t c)
{
    return (int8_t)SEGGER_RTT_PutChar(0, c);
}

#ifdef VIAL_ENABLE
#include "tusb.h"
#include "usb_common.h"

void raw_hid_send(uint8_t *data, uint8_t length)
{
    if (tud_hid_n_ready(ITF_NUM_VIAL)) {
        tud_hid_n_report(ITF_NUM_VIAL, 0, data, length);
    } else {
    }
}
#endif