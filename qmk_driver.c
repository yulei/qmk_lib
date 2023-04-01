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