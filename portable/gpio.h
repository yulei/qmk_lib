
#pragma once

#include "pin_defs.h"

#define setPinInput(pin) gpio_set_pin_input_floating((pin))
#define setPinInputHigh(pin) gpio_set_input_pullup((pin))
#define setPinInputLow(pin) gpio_set_input_pulldown((pin))
#define setPinOutputPushPull(pin) gpio_set_output_pushpull((pin))
#define setPinOutputOpenDrain(pin) gpio_set_output_opendrain((pin))
#define setPinOutput(pin) setPinOutputPushPull(pin)

#define writePinHigh(pin) gpio_write_pin(pin, 1)
#define writePinLow(pin) gpio_write_pin(pin, 0)
#define writePin(pin, level)   \
    do {                       \
        if (level) {           \
            writePinHigh(pin); \
        } else {               \
            writePinLow(pin);  \
        }                      \
    } while (0)

#define readPin(pin) gpio_read_pin(pin)

//#define togglePin(pin) palToggleLine(pin)
