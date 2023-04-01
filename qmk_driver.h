/**
 * @file qmk_driver.h
 * @author astro
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#pragma once

#include "suspend.h"
#include "report.h"
#include "host.h"
#include "action_util.h"
#include "mousekey.h"
#include "keyboard.h"
#include "wait.h"

void qmk_driver_init(void);
void qmk_driver_task(void);
