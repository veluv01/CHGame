/* Modified 2026-10-11: WeAct F8U6 hardware variant; see docs/weact-f8u6.md. */
/**
 *******************************************************************************
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * All rights reserved.
 *
 * This software component is licensed by WCH under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                        opensource.org/licenses/BSD-3-Clause
 *
 *******************************************************************************
 */

#include "pins_arduino.h"

const PinName digitalPin[] = {
  PA_0, PA_1, PA_2, PA_3, PA_4, PA_5, PA_6, PA_7,
  PB_0, PB_1, PB_3, PB_11, PB_12,
  PC_14, PC_15, PC_16, PC_17, PC_18, PC_19
};

const uint32_t analogInputPin[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
