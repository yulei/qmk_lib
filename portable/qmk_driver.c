/**
 * @file qmk_driver.c
 * @author astro
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include "qmk_driver.h"
#include "keyboard.h"

uint8_t keyboard_protocol = 1;

void qmk_driver_init(void)
{
    keyboard_setup();
    keyboard_init();
}

void qmk_driver_task(void)
{
    keyboard_task();
}

//#include "SEGGER_RTT.h"

int8_t sendchar(uint8_t c)
{
    return 1;
    //return (int8_t)SEGGER_RTT_PutChar(0, c);
}

__attribute__((weak))
void raw_hid_send_kb(uint8_t *data, uint8_t length) {}

#ifdef VIAL_ENABLE
#include "usb_common.h"
#ifdef TINYUSB_ENABLE
#include "tusb.h"

void raw_hid_send(uint8_t *data, uint8_t length)
{
    raw_hid_send_kb(data, length);
    if (tud_hid_n_ready(ITF_NUM_VIAL)) {
        tud_hid_n_report(ITF_NUM_VIAL, 0, data, length);
    } else {
    }
}
#else
#include "amk_usb.h"
void raw_hid_send(uint8_t *data, uint8_t length)
{
    raw_hid_send_kb(data, length);
    if (amk_usb_itf_ready(HID_REPORT_ID_VIAL)) {
        amk_usb_itf_send_report(HID_REPORT_ID_VIAL, data, length);
    } else {
    }
}
#endif
#endif

// for delay report
#include "usb_common.h"
#include "usb_interface.h"

void amk_report_delay(uint16_t delay)
{
   usb_send_report(HID_REPORT_ID_DELAY, &delay, sizeof(delay));
}
