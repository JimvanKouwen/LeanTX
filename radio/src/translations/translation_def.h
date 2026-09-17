/*
 * Copyright (C) EdgeTX
 *
 * Based on code named
 *   opentx - https://github.com/opentx/opentx
 *   th9x - http://code.google.com/p/th9x
 *   er9x - http://code.google.com/p/er9x
 *   gruvin9x - http://code.google.com/p/gruvin9x
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#pragma once

#if NARROW_LAYOUT
  #define TR3(x, y, z) y
  #define TR(x, y) y
#elif LCD_W >= 480
  #define TR3(x, y, z) z
  #define TR(x, y) y
#elif LCD_W >= 212
  #define TR3(x, y, z) y
  #define TR(x, y) y
#else
  #define TR3(x, y, z) x
  #define TR(x, y) x
#endif

#if defined(COLORLCD)
  #define BUTTON(x) x
  #define TR_BW_COL(x, y) y
#else
  #define BUTTON(x) "[" x "]"
  #define TR_BW_COL(x, y) x
#endif

#if (LCD_W == 212) || defined(COLORLCD)
 #define LCDW_128_LINEBREAK
#else
 #define LCDW_128_LINEBREAK        "\036"
#endif

#define TR_EMPTY                        "---"

// String array groups
#define TR_PPMUNIT                      "0.--","0.0","us"
#define TR_FUNCTION_SWITCH_GROUPS       "---", TR_SWITCH_GROUP" 1", TR_SWITCH_GROUP" 2", TR_SWITCH_GROUP" 3"
#define TR_CRSF_BAUDRATE                "115k","400k","921k","1.87M","3.75M","5.25M"
#define TR_MODULE_PROTOCOLS TR_OFF, TR_OFF, TR_OFF, TR_OFF, TR_OFF, "CRSF"
#define TR_SM_VSRCRAW "smA", "smB", "smC", "smD", "smE", "smF"
#define TR_PWR_OFF_DELAYS               "0s","0.5s","1s","2s","3s"
#define TR_SPLASHSCREEN_DELAYS          "1s","2s","3s","4s","6s","8s","10s","15s"
#define TR_FSGROUPS                     "-","1","2","3","4"

#define TR_TIMER_MODES      TR_OFFON,TR_START,TR_THROTTLE_LABEL,TR_THROTTLE_PERCENT_LABEL,TR_THROTTLE_START
#define TR_PHASES_HEADERS   TR_PHASES_HEADERS_NAME, TR_PHASES_HEADERS_SW, TR_PHASES_HEADERS_FAD_IN, TR_PHASES_HEADERS_FAD_OUT
#define TR_LIMITS_HEADERS   TR_LIMITS_HEADERS_NAME, TR_LIMITS_HEADERS_SUBTRIM, TR_LIMITS_HEADERS_MIN, TR_LIMITS_HEADERS_MAX, TR_LIMITS_HEADERS_DIRECTION, TR_LIMITS_HEADERS_PPMCENTER, TR_LIMITS_HEADERS_SUBTRIMMODE

#define SA2(s) s##_1, s##_2
#define SA3(s) s##_1, s##_2, s##_3
#define SA4(s) s##_1, s##_2, s##_3, s##_4
#define SA5(s) s##_1, s##_2, s##_3, s##_4, s##_5
#define SA6(s) s##_1, s##_2, s##_3, s##_4, s##_5, s##_6
#define SA7(s) s##_1, s##_2, s##_3, s##_4, s##_5, s##_6, s##_7
#define SA8(s) s##_1, s##_2, s##_3, s##_4, s##_5, s##_6, s##_7, s##_8
#define SA9(s) s##_1, s##_2, s##_3, s##_4, s##_5, s##_6, s##_7, s##_8, s##_9
#define SA10(s) s##_1, s##_2, s##_3, s##_4, s##_5, s##_6, s##_7, s##_8, s##_9, s##_10
#define SA11(s) s##_1, s##_2, s##_3, s##_4, s##_5, s##_6, s##_7, s##_8, s##_9, s##_10, s##_11
#define SA12(s) s##_1, s##_2, s##_3, s##_4, s##_5, s##_6, s##_7, s##_8, s##_9, s##_10, s##_11, s##_12
#define SA16(s) s##_1, s##_2, s##_3, s##_4, s##_5, s##_6, s##_7, s##_8, s##_9, s##_10, s##_11, s##_12, s##_13, s##_14, s##_15, s##_16
#define SA30(s) s##_1, s##_2, s##_3, s##_4, s##_5, s##_6, s##_7, s##_8, s##_9, s##_10, s##_11, s##_12, s##_13, s##_14, s##_15, s##_16, \
                s##_17, s##_18, s##_19, s##_20, s##_21, s##_22, s##_23, s##_24, s##_25, s##_26, s##_27, s##_28, s##_29, s##_30

#define TR_VTELEMUNIT               SA30(TR_VTELEMUNIT)

#define TR_MONTHS                   SA12(TR_MONTHS)

#define TR_VCELLINDEX               SA11(TR_VCELLINDEX)

#define TR_AUX_SERIAL_MODES         SA11(TR_AUX_SERIAL_MODES)

#define TR_VFORMULAS                SA9(TR_VFORMULAS)
#define TR_FS_COLOR_LIST            SA9(TR_FS_COLOR_LIST)
#define TR_VUSBJOYSTICK_CH_AXIS     SA9(TR_VUSBJOYSTICK_CH_AXIS)

#define TR_POTTYPES                 SA8(TR_POTTYPES)
#define TR_VUSBJOYSTICK_CH_SWPOS    SA8(TR_VUSBJOYSTICK_CH_SWPOS)
#define TR_VUSBJOYSTICK_CH_SIM      SA8(TR_VUSBJOYSTICK_CH_SIM)

#define TR_FONT_SIZES               SA8(TR_FONT_SIZES)

#define TR_VBEEPCOUNTDOWN           SA6(TR_VBEEPCOUNTDOWN)
#define TR_VTMRMODES                SA6(TR_VTMRMODES)

#define TR_VBLMODE                  SA5(TR_VBLMODE)
#if defined(FUNCTION_SWITCHES)
#define TR_SWTYPES                  SA5(TR_SWTYPES)
#else
#define TR_SWTYPES                  SA4(TR_SWTYPES)
#endif
#if defined(COLORLCD)
#define TR_ROTARY_ENC_OPT           SA2(TR_ROTARY_ENC_OPT)
#else
#define TR_ROTARY_ENC_OPT           SA5(TR_ROTARY_ENC_OPT)
#endif
#define TR_VUSBJOYSTICK_CH_BTNMODE  SA5(TR_VUSBJOYSTICK_CH_BTNMODE)
#define TR_VUSBJOYSTICK_CH_BTNMODE_S SA5(TR_VUSBJOYSTICK_CH_BTNMODE_S)

#define TR_VBEEPMODE                SA4(TR_VBEEPMODE)
#define TR_USBMODES                 SA4(TR_USBMODES)
#define TR_COUNTDOWNVALUES          SA4(TR_COUNTDOWNVALUES)
#define TR_VTELEMSCREENTYPE         SA4(TR_VTELEMSCREENTYPE)
#define TR_HATSOPT                  SA4(TR_HATSOPT)
#if defined(PCBX12S)
#define TR_ANTENNA_MODES            SA4(TR_ANTENNA_MODES)
#else
#define TR_ANTENNA_MODES            TR_ANTENNA_MODES_1, TR_ANTENNA_MODES_2, TR_ANTENNA_MODES_3, TR_ANTENNA_MODES_5
#endif
#define TR_VUSBJOYSTICK_CH_MODE     SA4(TR_VUSBJOYSTICK_CH_MODE)
#define TR_VUSBJOYSTICK_CH_MODE_S   SA4(TR_VUSBJOYSTICK_CH_MODE_S)
#define TR_VUSBJOYSTICK_CIRC_COUTOUT SA4(TR_VUSBJOYSTICK_CIRC_COUTOUT)
#define TR_SORT_ORDERS              SA4(TR_SORT_ORDERS)

#define TR_VPERSISTENT              SA3(TR_VPERSISTENT)
#define TR_COUNTRY_CODES            SA3(TR_COUNTRY_CODES)
#define TR_ADCFILTERVALUES          SA3(TR_ADCFILTERVALUES)
#define TR_CYC_VSRCRAW              "[C1]","[C2]","[C3]"
#define TR_VPREC                    SA3(TR_VPREC)
#if defined(PCBX9E)
#define TR_BLUETOOTH_MODES          TR_BLUETOOTH_MODES_1, TR_BLUETOOTH_MODES_4
#else
#define TR_BLUETOOTH_MODES          SA2(TR_BLUETOOTH_MODES)
#endif
#define TR_PREFLIGHT_POTSLIDER_CHECK SA3(TR_PREFLIGHT_POTSLIDER_CHECK)
#define TR_ALIGN_OPTS               SA3(TR_ALIGN_OPTS)
#define TR_VUSBJOYSTICK_IF_MODE     SA3(TR_VUSBJOYSTICK_IF_MODE)

#define TR_OFFON                    SA2(TR_OFFON)
#define TR_MMMINV                   SA2(TR_MMMINV)
#define TR_VVARIOCENTER             SA2(TR_VVARIOCENTER)
#define TR_VUNITSSYSTEM             SA2(TR_VUNITSSYSTEM)
#define TR_GPSFORMAT                SA2(TR_GPSFORMAT)
#define TR_ON_ONE_SWITCHES          SA2(TR_ON_ONE_SWITCHES)
#define TR_VSENSORTYPES             SA2(TR_VSENSORTYPES)
#define TR_TIMER_DIR                SA2(TR_TIMER_DIR)
#define TR_SAMPLE_MODES             SA2(TR_SAMPLE_MODES)
#define TR_LABELS_SELECT_MODE       SA2(TR_LABELS_SELECT_MODE)
#define TR_LABELS_MATCH_MODE        SA2(TR_LABELS_MATCH_MODE)
#define TR_FAV_MATCH_MODE SA2(TR_FAV_MATCH_MODE)
#define TR_VUSBJOYSTICK_EXTMODE     SA2(TR_VUSBJOYSTICK_EXTMODE)
#define TR_SUBTRIMMODES             SA2(TR_SUBTRIMMODES)
#define TR_IMU_VSRCRAW              SA2(TR_IMU_VSRCRAW)

#define TR_EXIT_BTN     BUTTON(TR_EXIT)
#define TR_MAIN_VIEW_1  TR_MAIN_VIEW_X "1"
#define TR_MAIN_VIEW_2  TR_MAIN_VIEW_X "2"
#define TR_MAIN_VIEW_3  TR_MAIN_VIEW_X "3"
#define TR_MAIN_VIEW_4  TR_MAIN_VIEW_X "4"
#define TR_MAIN_VIEW_5  TR_MAIN_VIEW_X "5"
#define TR_MAIN_VIEW_6  TR_MAIN_VIEW_X "6"
#define TR_MAIN_VIEW_7  TR_MAIN_VIEW_X "7"
#define TR_MAIN_VIEW_8  TR_MAIN_VIEW_X "8"
#define TR_MAIN_VIEW_9  TR_MAIN_VIEW_X "9"
#define TR_MAIN_VIEW_10 TR_MAIN_VIEW_X "10"
#define TR_TIMER_1      TR_TIMER "1"
#define TR_TIMER_2      TR_TIMER "2"
#define TR_TIMER_3      TR_TIMER "3"

#if !defined(COLORLCD)
#define TR_POPUPS_ENTER_EXIT  TR_BW_COL(TR(TR_EXIT "\010" TR_ENTER, TR_EXIT "\010\010\010\010" TR_ENTER), TR_ENTER "\010" TR_EXIT)
#endif
