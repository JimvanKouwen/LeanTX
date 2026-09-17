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
 *
 */

/*
 * Formatting octal codes available in TR_ strings:
 *  \037\x           -sets LCD x-coord (x value in octal)
 *  \036             -newline
 *  \035             -horizontal tab (ARM only)
 *  \001 to \034     -extended spacing (value * FW/2)
 *  \0               -ends current string
 */

// Main menu
#define TR_QM_MANAGE_MODELS             "Manage\nModels"
#define TR_QM_MODEL_SETUP               "Model\nSetup"
#define TR_QM_RADIO_SETUP               "Radio\nSetup"
#define TR_QM_UI_SETUP                  "UI\nSetup"
#define TR_QM_TOOLS                     "Tools"
#define TR_QM_MODEL_SETTINGS            "Model\nSettings"
#define TR_QM_RADIO_SETTINGS            "Radio\nSettings"
#define TR_QM_TELEM                     "Telemetry"
#define TR_QM_HARDWARE                  "Hardware"
#define TR_QM_ABOUT                     "About\nEdgeTX"
#define TR_QM_THEMES                    "Themes"
#define TR_QM_TOP_BAR                   "Top Bar"
#define TR_QM_SCREEN_1                  "Screen 1"
#define TR_QM_SCREEN_2                  "Screen 2"
#define TR_QM_SCREEN_3                  "Screen 3"
#define TR_QM_SCREEN_4                  "Screen 4"
#define TR_QM_SCREEN_5                  "Screen 5"
#define TR_QM_SCREEN_6                  "Screen 6"
#define TR_QM_SCREEN_7                  "Screen 7"
#define TR_QM_SCREEN_8                  "Screen 8"
#define TR_QM_SCREEN_9                  "Screen 9"
#define TR_QM_SCREEN_10                 "Screen 10"
#define TR_QM_ADD_SCREEN                "Add\nScreen"
#define TR_QM_APPS                      "Apps"
#define TR_QM_STORAGE                   "Storage"
#define TR_QM_RESET                     "Flight\nReset"
#define TR_QM_CHAN_MON                  "Channel\nMonitor"
#define TR_QM_STATS                     "Statistics"
#define TR_QM_DEBUG                     "Debug"
#define TR_MAIN_MODEL_SETTINGS          "Model Settings"
#define TR_MAIN_RADIO_SETTINGS          "Radio Settings"
#define TR_MAIN_MENU_MANAGE_MODELS      "Manage Models"
#define TR_MAIN_MENU_MODEL_NOTES        "Model Notes"
#define TR_MAIN_MENU_CHANNEL_MONITOR    "Channel Monitor"
#define TR_MAIN_MENU_MODEL_SETTINGS     "Model Setup"
#define TR_MAIN_MENU_RADIO_SETTINGS     "Radio Setup"
#define TR_MAIN_MENU_SCREEN_SETTINGS    "UI Setup"
#define TR_MAIN_MENU_STATISTICS         "Statistics"
#define TR_MAIN_MENU_ABOUT_EDGETX       "About EdgeTX"
#define TR_MAIN_VIEW_X                  "Screen "
#define TR_MAIN_MENU_THEMES             "Themes"
#define TR_MAIN_MENU_APPS               "Apps"
#define TR_MENUTELEMETRY                TR_BW_COL("TELEMETRY", "Telemetry")
#define TR_HARDWARE                     TR_BW_COL("HARDWARE", "Hardware")
#define TR_USER_INTERFACE               "Top Bar"
#define TR_SD_CARD                      TR_BW_COL("SD CARD", "Storage")
#define TR_DEBUG                        "Debug"
#define TR_MENU_RADIO_SWITCHES          TR_BW_COL("SWITCHES", "Switches Test")
#define TR_MENUCALIBRATION              TR_BW_COL("CALIBRATION", "Calibration")
#define TR_FUNCTION_SWITCHES            "Customizable Switches"
// End Main menu

#define TR_MINUTE_SINGULAR            "minute"
#define TR_MINUTE_PLURAL1             "minutes"
#define TR_MINUTE_PLURAL2             "minutes"

#define TR_OFFON_1                     "OFF"
#define TR_OFFON_2                     "ON"
#define TR_MMMINV_1                    "---"
#define TR_MMMINV_2                    "INV"
#define TR_VBEEPMODE_1                 "Quiet"
#define TR_VBEEPMODE_2                 "Alarm"
#define TR_VBEEPMODE_3                 "NoKey"
#define TR_VBEEPMODE_4                 "All"
#define TR_VBLMODE_1                   "OFF"
#define TR_VBLMODE_2                   "Keys"
#define TR_VBLMODE_3                   TR("Ctrl","Controls")
#define TR_VBLMODE_4                   "Both"
#define TR_VBLMODE_5                   "ON"

#define TR_AUX_SERIAL_MODES_1          "OFF"
#define TR_AUX_SERIAL_MODES_2          "Telem Mirror"
#define TR_AUX_SERIAL_MODES_3          "Telemetry In"
#define TR_AUX_SERIAL_MODES_4          "---"
#define TR_AUX_SERIAL_MODES_5          "---"
#define TR_AUX_SERIAL_MODES_6          "LUA"
#define TR_AUX_SERIAL_MODES_7          "CLI"
#define TR_AUX_SERIAL_MODES_8          "GPS"
#define TR_AUX_SERIAL_MODES_9          "Debug"
#define TR_AUX_SERIAL_MODES_10         "SpaceMouse"
#define TR_AUX_SERIAL_MODES_11         "External module"
#define TR_SWTYPES_1                   "None"
#define TR_SWTYPES_2                   "Toggle"
#define TR_SWTYPES_3                   "2POS"
#define TR_SWTYPES_4                   "3POS"
#define TR_SWTYPES_5                   "Global"
#define TR_POTTYPES_1                  "None"
#define TR_POTTYPES_2                  "Pot"
#define TR_POTTYPES_3                  TR("Pot w. det","Pot with detent")
#define TR_POTTYPES_4                  "Slider"
#define TR_POTTYPES_5                  TR("Multipos","Multipos Switch")
#define TR_POTTYPES_6                  "Axis X"
#define TR_POTTYPES_7                  "Axis Y"
#define TR_POTTYPES_8                  "Switch"
#define TR_VPERSISTENT_1               "OFF"
#define TR_VPERSISTENT_2               "Flight"
#define TR_VPERSISTENT_3               "Manual Reset"
#define TR_COUNTRY_CODES_1             TR("US","America")
#define TR_COUNTRY_CODES_2             TR("JP","Japan")
#define TR_COUNTRY_CODES_3             TR("EU","Europe")
#define TR_USBMODES_1                  "Ask"
#define TR_USBMODES_2                  TR("Joyst","Joystick")
#define TR_USBMODES_3                  TR("SDCard","Storage")
#define TR_USBMODES_4                  "Serial"

#define TR_VBEEPCOUNTDOWN_1            "Silent"
#define TR_VBEEPCOUNTDOWN_2            "Beeps"
#define TR_VBEEPCOUNTDOWN_3            "Voice"
#define TR_VBEEPCOUNTDOWN_4            "Haptic"
#define TR_VBEEPCOUNTDOWN_5            TR("B & H","Beeps & Haptic")
#define TR_VBEEPCOUNTDOWN_6            TR("V & H","Voice & Haptic")
#define TR_COUNTDOWNVALUES_1           "5s"
#define TR_COUNTDOWNVALUES_2           "10s"
#define TR_COUNTDOWNVALUES_3           "20s"
#define TR_COUNTDOWNVALUES_4           "30s"
#define TR_VVARIOCENTER_1              "Tone"
#define TR_VVARIOCENTER_2              "Silent"

#define TR_ADCFILTERVALUES_1           "Global"
#define TR_ADCFILTERVALUES_2           "Off"
#define TR_ADCFILTERVALUES_3           "On"

#define TR_VUNITSSYSTEM_1              "Metric"
#define TR_VUNITSSYSTEM_2              TR("Imper.","Imperial")
#define TR_VTELEMUNIT_1                "-"
#define TR_VTELEMUNIT_2                "V"
#define TR_VTELEMUNIT_3                "A"
#define TR_VTELEMUNIT_4                "mA"
#define TR_VTELEMUNIT_5                "kts"
#define TR_VTELEMUNIT_6                "m/s"
#define TR_VTELEMUNIT_7                "f/s"
#define TR_VTELEMUNIT_8                "kmh"
#define TR_VTELEMUNIT_9                "mph"
#define TR_VTELEMUNIT_10               "m"
#define TR_VTELEMUNIT_11               "ft"
#define TR_VTELEMUNIT_12               "°C"
#define TR_VTELEMUNIT_13               "°F"
#define TR_VTELEMUNIT_14               "%"
#define TR_VTELEMUNIT_15               "mAh"
#define TR_VTELEMUNIT_16               "W"
#define TR_VTELEMUNIT_17               "mW"
#define TR_VTELEMUNIT_18               "dB"
#define TR_VTELEMUNIT_19               "rpm"
#define TR_VTELEMUNIT_20               "g"
#define TR_VTELEMUNIT_21               "°"
#define TR_VTELEMUNIT_22               "rad"
#define TR_VTELEMUNIT_23               "ml"
#define TR_VTELEMUNIT_24               "fOz"
#define TR_VTELEMUNIT_25               "mlm"
#define TR_VTELEMUNIT_26               "Hz"
#define TR_VTELEMUNIT_27               "ms"
#define TR_VTELEMUNIT_28               "us"
#define TR_VTELEMUNIT_29               "km"
#define TR_VTELEMUNIT_30               "dBm"

#define TR_VTELEMSCREENTYPE_1          "None"
#define TR_VTELEMSCREENTYPE_2          "Nums"
#define TR_VTELEMSCREENTYPE_3          "Bars"
#define TR_VTELEMSCREENTYPE_4          "Script"
#define TR_GPSFORMAT_1                 "DMS"
#define TR_GPSFORMAT_2                 "NMEA"

#define TR_STICK_NAMES0                "Rud"
#define TR_STICK_NAMES1                "Ele"
#define TR_STICK_NAMES2                "Thr"
#define TR_STICK_NAMES3                "Ail"

#define TR_ON_ONE_SWITCHES_1           "ON"
#define TR_ON_ONE_SWITCHES_2           "One"

#define TR_HATSMODE                    "Hats mode"
#define TR_HATSOPT_1                   "Buttons only"
#define TR_HATSOPT_2                   "Keys only"
#define TR_HATSOPT_3                   "Switchable"
#define TR_HATSOPT_4                   "Global"
#define TR_HATSMODE_TRIMS              "Hats mode: Buttons"
#define TR_HATSMODE_KEYS               "Hats mode: Keys"
#define TR_HATSMODE_KEYS_HELP          "Left side:\n"\
                                       "   Right = MDL\n"\
                                       "   Up = SYS\n"\
                                       "   Down = TELE\n"\
                                       "\n"\
                                       "Right side:\n"\
                                       "   Left = PAGE<\n"\
                                       "   Right = PAGE>\n"\
                                       "   Up = PREV/INC\n"\
                                       "   Down = NEXT/DEC"

#define TR_ROTARY_ENC_OPT_1       "Normal"
#define TR_ROTARY_ENC_OPT_2       "Inverted"
#define TR_ROTARY_ENC_OPT_3       "V-I H-N"
#define TR_ROTARY_ENC_OPT_4       "V-I H-A"
#define TR_ROTARY_ENC_OPT_5       "V-N E-I"

#define TR_IMU_VSRCRAW_1             "TltX"
#define TR_IMU_VSRCRAW_2             "TltY"

#define TR_SRC_BATT                    "Batt"
#define TR_SRC_TIME                    "Time"
#define TR_SRC_GPS                     "GPS"
#define	TR_SRC_LIGHT                    "Ambient light"
#define TR_SRC_TIMER                   "Tmr"

#define TR_VTMRMODES_1                 "OFF"
#define TR_VTMRMODES_2                 "ON"
#define TR_VTMRMODES_3                 "Strt"
#define TR_VTMRMODES_4                 "THs"
#define TR_VTMRMODES_5                 "TH%"
#define TR_VTMRMODES_6                 "THt"
#define TR_VSENSORTYPES_1              "Custom"
#define TR_VSENSORTYPES_2              "Calculated"
#define TR_VFORMULAS_1                 "Add"
#define TR_VFORMULAS_2                 "Average"
#define TR_VFORMULAS_3                 "Min"
#define TR_VFORMULAS_4                 "Max"
#define TR_VFORMULAS_5                 "Multiply"
#define TR_VFORMULAS_6                 "Totalize"
#define TR_VFORMULAS_7                 "Cell"
#define TR_VFORMULAS_8                 "Consumpt"
#define TR_VFORMULAS_9                 "Distance"
#define TR_VPREC_1                     "0.--"
#define TR_VPREC_2                     "0.0 "
#define TR_VPREC_3                     "0.00"
#define TR_VCELLINDEX_1                "Lowest"
#define TR_VCELLINDEX_2                "1"
#define TR_VCELLINDEX_3                "2"
#define TR_VCELLINDEX_4                "3"
#define TR_VCELLINDEX_5                "4"
#define TR_VCELLINDEX_6                "5"
#define TR_VCELLINDEX_7                "6"
#define TR_VCELLINDEX_8                "7"
#define TR_VCELLINDEX_9                "8"
#define TR_VCELLINDEX_10               "Highest"
#define TR_VCELLINDEX_11               "Delta"
#define TR_TIMER_DIR_1                 TR("Remain", "Show Remain")
#define TR_TIMER_DIR_2                 TR("Elaps.", "Show Elapsed")

#define TR_FONT_SIZES_1                "STD"
#define TR_FONT_SIZES_2                "BOLD"
#define TR_FONT_SIZES_3                "XXS"
#define TR_FONT_SIZES_4                "XS"
#define TR_FONT_SIZES_5                "L"
#define TR_FONT_SIZES_6                "XL"
#define TR_FONT_SIZES_7                "XXL"
#define TR_FONT_SIZES_8                "LXL"

#define TR_ENTER                       "[ENTER]"
#define TR_OK                          TR_BW_COL(TR("\010\010\010[OK]", "\010\010\010\010\010[OK]"), "Ok")
#define TR_EXIT                        TR_BW_COL("EXIT", "RTN")

#define TR_YES                         "Yes"
#define TR_NO                          "No"
#define TR_DELETEMODEL                 "DELETE MODEL"
#define TR_COPYINGMODEL                "Copying model..."
#define TR_MOVINGMODEL                 "Moving model..."
#define TR_LOADINGMODEL                "Loading model..."
#define TR_UNLABELEDMODEL              "Unlabeled"
#define TR_NAME                        "Name"
#define TR_MODELNAME                   "Model name"
#define TR_PHASENAME                   "Mode name"
#define TR_MIXNAME                     "Mix name"
#define TR_BITMAP                      "Model image"
#define TR_NO_PICTURE                  "No Picture"
#define TR_TIMER                       TR("Timer", "Timer ")
#define TR_NO_TIMERS                   "No timers"
#define TR_START                       "Start"
#define TR_NEXT                        "Next"
#define TR_TTRACE                      TR("T-Source", "Source")
#define TR_BEEPCTR                     TR("Ctr Beep", "Beep when centered")
#define TR_PROTOCOL                    TR("Proto", "Protocol")
  #define TR_REFRESHRATE               TR("Refresh", "Refresh rate")
  #define TR_WARN_BATTVOLTAGE         TR("Output is VBAT: ", "Warning: output level is VBAT: ")
#define TR_WARN_5VOLTS                 "Warning: output level is 5 volts"
#define TR_MS                          "ms"
#define TR_SWITCH                      "Switch"
#define TR_FS_COLOR_LIST_1             "Custom"
#define TR_FS_COLOR_LIST_2             "Off"
#define TR_FS_COLOR_LIST_3             "White"
#define TR_FS_COLOR_LIST_4             "Red"
#define TR_FS_COLOR_LIST_5             "Green"
#define TR_FS_COLOR_LIST_6             "Yellow"
#define TR_FS_COLOR_LIST_7             "Orange"
#define TR_FS_COLOR_LIST_8             "Blue"
#define TR_FS_COLOR_LIST_9             "Pink"
#define TR_GROUP                       "Group"
#define TR_GROUP_ALWAYS_ON             "Always on"
#define TR_LUA_OVERRIDE                "Allow Lua override"
#define TR_LAST                        "Last"
#define TR_MORE_INFO                   "More info"
#define TR_SWITCH_TYPE                 "Type"
#define TR_SWITCH_STARTUP              "Startup"
#define TR_SWITCH_GROUP                "Group"
#define TR_TRIMS                       "Trims"
#define TR_MODE                        "Mode"
#define TR_LEFT_STICK                  "Left"
#define TR_SUBTYPE                     "Subtype"
#define TR_SOURCE                       "Source"
#define TR_WEIGHT                      "Weight"
#define TR_OFFSET                       "Offset"
#define TR_OFF                         "OFF"
#define TR_ANTENNA                     "Antenna"
#define TR_NO_INFORMATION              TR("No info", "No information")
#define TR_RANGE                       "Range"
#define TR_CENTER                      "Center"
#define TR_ALARM                       "Alarm"
#define TR_BLADES                      "Blades/Poles"
#define TR_SCREEN                      "Screen\001"
#define TR_SOUND_LABEL                 "Sound"
#define TR_LENGTH                      "Length"
#define TR_BEEP_LENGTH                 "Beep length"
#define TR_BEEP_PITCH                  "Beep pitch"
#define TR_HAPTIC_LABEL                "Haptic"
#define TR_STRENGTH                    "Strength"
#define TR_IMU_LABEL                   "IMU"
#define TR_IMU_OFFSET                  "Offset"
#define TR_IMU_MAX                     "Max"
#define TR_CONTRAST                    "Contrast"
#define TR_ALARMS_LABEL                "Alarms"
#define TR_BATTERY_RANGE               TR("Batt. range", "Battery meter range")
#define TR_BATTERYCHARGING             "Charging..."
#define TR_BATTERYFULL                 "Battery full"
#define TR_BATTERYNONE                 "None!"
#define TR_BATTERYWARNING              "Battery low"
#define TR_INACTIVITYALARM             "Inactivity"
#define TR_MEMORYWARNING               "Memory low"
#define TR_ALARMWARNING                "Sound off"
#define TR_RSSI_SHUTDOWN_ALARM         TR("RSSI shutdown", "Check RSSI on shutdown")
#define TR_MODEL_STILL_POWERED         "Model still powered"
#define TR_USB_STILL_CONNECTED         "USB still connected"
#define TR_MODEL_SHUTDOWN              "Shutdown?"
#define TR_PRESS_ENTER_TO_CONFIRM      "Press enter to confirm"
#define TR_THROTTLE_LABEL              "Throttle"
#define TR_THROTTLE_START              "Throttle Start"
#define TR_MINUTEBEEP                  TR("Minute", "Minute call")
#define TR_BEEPCOUNTDOWN               "Countdown"
#define TR_PERSISTENT                  TR("Persist.", "Persistent")
#define TR_BACKLIGHT_LABEL             "Backlight"
#define TR_STATUS                      "Status"
#define TR_BLONBRIGHTNESS              "ON brightness"
#define TR_BLOFFBRIGHTNESS             "OFF brightness"
#define TR_KEYS_BACKLIGHT              "Keys backlight"
#define TR_BLCOLOR                     "Color"
#define TR_ONE_LOG_PER_DAY             "One log per day"
#define TR_KEY_LOCK_FMT                "Key lock (%s+%s hold)"
#define TR_KEYS_LOCKED                 "Keys locked"
#define TR_KEYS_LOCKED_FMT             TR_BW_COL("%s+%s to unlock", "Keys locked (%s+%s to unlock)")
#define TR_KEYS_UNLOCKED               "Keys unlocked"
#define TR_TOUCH_ENABLED               "Touch screen enabled"
#define TR_TOUCH_DISABLED              "Touch screen disabled"
#define TR_SPLASHSCREEN                "Splash screen"
#define TR_PLAY_HELLO                  "Startup Sound"
#define TR_PWR_ON_DELAY                "Pwr On delay"
#define TR_PWR_OFF_DELAY               "Pwr Off delay"
#define TR_PWR_AUTO_OFF                TR("Pwr Auto Off","Power Auto Off")
#define TR_PWR_ON_OFF_HAPTIC           TR("Pwr ON/OFF Haptic","Power ON/OFF Haptic")
#define TR_THROTTLE_WARNING            TR("T-Warning", "Throttle state")
#define TR_CUSTOM_THROTTLE_WARNING     TR("Cust-Pos", "Custom position?")
#define TR_CUSTOM_THROTTLE_WARNING_VAL TR("Pos. %", "Position %")
#define TR_SWITCHWARNING               TR("S-Warning", "Switch positions")
#define TR_POTWARNINGSTATE             "Pots & sliders"
#define TR_POTWARNING                  TR("Pot warn.", "Pot positions")
#define TR_TIMEZONE                    "Time zone"
#define TR_ADJUST_RTC                  "Adjust RTC"
#define TR_GPS                         "GPS"
#define TR_DEF_CHAN_ORD                TR("Def chan order", "Default channel order")
#define TR_STICKS                      "Axis"
#define TR_POTS                        "Pots"
#define TR_SWITCHES                    "Switches"
#define TR_SWITCHES_DELAY              TR("Play delay", "Play delay (sw. mid pos)")
#define TR_SLAVE                       "Slave"
#define TR_MULTIPLIER                  "Multiplier"
#define TR_CAL                         "Cal"
#define TR_CALIBRATION                 BUTTON("Calibration")
#define TR_VTRIM                       "Trim - +"
#define TR_CALIB_DONE                  "Calibration completed"
#define TR_MENUTOSTART                 TR_ENTER " TO START"
#define TR_MENUWHENDONE                TR_ENTER " WHEN DONE"
#define TR_AXISDIR                     "AXIS DIR"
#define TR_MENUAXISDIR                 "[ENTER LONG] " TR_AXISDIR
#define TR_SETMIDPOINT                 TR_BW_COL(TR("SET AXIS MIDPOINT", "CENTER AXIS/SLIDERS"), "CENTER AXIS/SLIDERS")
#define TR_MOVESTICKSPOTS              TR_BW_COL("MOVE AXIS/POTS", "MOVE AXIS/POTS")
#define TR_NODATA                      "NO DATA"
#define TR_US                          "us"
#define TR_HZ                          "Hz"
#define TR_TMIXMAXMS                   "Tmix max"
#define TR_FREE_STACK                  "Free stack"
#define TR_INT_GPS_LABEL               "Internal GPS"
#define TR_HEARTBEAT_LABEL             "Heartbeat"
#define TR_LUA_SCRIPTS_LABEL           "Lua scripts"
#define TR_FREE_MEM_LABEL              "Free mem"
#define TR_DURATION_MS             TR("[D]","Duration(ms): ")
#define TR_INTERVAL_MS             TR("[I]","Interval(ms): ")
#define TR_MEM_USED_SCRIPT         "Script(B): "
#define TR_MEM_USED_WIDGET         "Widget(B): "
#define TR_MEM_USED_EXTRA          "Extra(B): "
#define TR_STACK_MIX                   "Mix: "
#define TR_STACK_AUDIO                 "Audio: "
#define TR_GPS_FIX_YES                 "Fix: Yes"
#define TR_GPS_FIX_NO                  "Fix: No"
#define TR_GPS_SATS                    "Sats: "
#define TR_GPS_HDOP                    "Hdop: "
#define TR_STACK_MENU                  "Menu: "
#define TR_TIMER_LABEL                 "Timer"
#define TR_THROTTLE_PERCENT_LABEL      "Throttle %"
#define TR_BATT_LABEL                  "Battery"
#define TR_SESSION                     "Session"
#define TR_MENUTORESET                 TR_ENTER " to reset"
#define TR_CH                          "CH"
#define TR_MODEL                       "MODEL"
#define TR_PRESS_ANY_KEY_TO_SKIP       "Press any key to skip"
#define TR_THROTTLE_NOT_IDLE           "Throttle not idle"
#define TR_ALARMSDISABLED              "Alarms disabled"
#define TR_PRESSANYKEY                 TR("\010Press any Key", "Press any key")
#define TR_BAD_RADIO_DATA              "Missing or bad radio data"
#define TR_RADIO_DATA_RECOVERED        TR3("Using backup radio data","Using backup radio settings","Radio settings recovered from backup")
#define TR_RADIO_DATA_UNRECOVERABLE    TR3("Radio settings invalid","Radio settings not valid", "Unable to read valid radio settings")
#define TR_STORAGE_FORMAT              "Storage preparation"
#define TR_RADIO_SETUP                 "RADIO SETUP"
#define TR_MENUVERSION                 "VERSION"
#define TR_MENU_RADIO_ANALOGS_CALIB    "CALIBRATED ANALOGS"
#define TR_MENU_RADIO_ANALOGS_RAWLOWFPS "RAW ANALOGS (5 Hz)"
#define TR_MENU_FSWITCH                "CUSTOMIZABLE SWITCHES"
#define TR_MENUMODELSEL                TR("MODELSEL", "MODEL SELECTION")
#define TR_MENU_MODEL_SETUP            TR("SETUP", "MODEL SETUP")

#define TR_MENUSTAT                    "STATS"
#define TR_MENUDEBUG                   "DEBUG"
#define TR_MONITOR_CHANNELS            "CHANNELS %d-%d"
#define TR_MONITOR_OUTPUT_DESC         "Outputs"
#define TR_MONITOR_MIXER_DESC          "Mixers"
  #define TR_RECEIVER_NUM              TR("RxNum", "Receiver No.")
#define TR_RECEIVER "Receiver"
#define TR_RF_POWER "RF Power"
#define TR_DISABLE_CH_MAP              TR("No Ch. map", "Disable Ch. map")
#define TR_DISABLE_TELEM TR("No Telem", "Disable Telemetry")
#define TR_MODULE_TELEMETRY            TR("S.Port", "S.Port link")
#define TR_MODULE_TELEM_ON             TR("ON", "Enabled")
#define TR_DISABLE_INTERNAL            TR("Disable int.", "Disable internal RF")
#define TR_MODULE_NO_SERIAL_MODE       TR("!serial mode", "Not in serial mode")
#define TR_MODULE_NO_INPUT             TR("No input", "No serial input")
#define TR_MODULE_NO_TELEMETRY         TR3("No telemetry", "No MULTI_TELEMETRY", "No MULTI_TELEMETRY detected")
#define TR_MODULE_WAITFORBIND          "Bind to load protocol"
#define TR_MODULE_BINDING              TR("Bind...","Binding")
#define TR_MODULE_UPGRADE_ALERT        TR3("Upg. needed", "Module upgrade required", "Module\nUpgrade required")
#define TR_MODULE_UPGRADE              TR("Upg. advised", "Module update recommended")
#define TR_REBIND                      "Rebinding required"
#define TR_REG_OK                      "Registration ok"
#define TR_BIND_OK "Bind successful"
#define TR_PROTOCOL_INVALID            TR("Prot. invalid", "Protocol invalid")
#define TR_MODULE_STATUS               TR("Status", "Module Status")
#define TR_MODULE_SYNC TR("Sync", "Proto Sync Status")
#define TR_IBUS2_SENSORS_MODE_ONLY     "Only in the iBUS2 mode can the sensors be set."
#define TR_GPS_COORDS_FORMAT           TR("GPS Coords", "Coordinate format")
#define TR_VARIO                       TR("Vario", "Variometer")
#define TR_PITCH_AT_ZERO               "Pitch zero"
#define TR_PITCH_AT_MAX                "Pitch max"
#define TR_REPEAT_AT_ZERO              "Repeat zero"
#define TR_BATT_CALIB                  TR("Batt. calib", "Battery calibration")
#define TR_VOLTAGE                     TR("Voltage", "Voltage source")
#define TR_SELECT_MODEL                "Select model"
#define TR_SELECT_MODE                 "Select mode"
#define TR_CREATE_MODEL                "Create model"
#define TR_FAVORITE_LABEL              "Favorites"
#define TR_MODELS_MOVED                "Unused models moved to"
#define TR_NEW_MODEL                   "New Model"
#define TR_LABEL_MODEL                 "Label model"
#define TR_MOVE_UP                     "Move Up"
#define TR_MOVE_DOWN                   "Move Down"
#define TR_ENTER_LABEL                 "Enter Label"
#define TR_LABELS                      "Labels"
#define TR_ACTIVE                      "Active"
#define TR_NEW                         "New"
#define TR_NEW_LABEL                   "New Label"
#define TR_RENAME_LABEL                "Rename Label"
#define TR_DELETE_LABEL                "Delete Label"
#define TR_DUPLICATE_MODEL             "Duplicate model"
#define TR_COPY_MODEL                  "Copy model"
#define TR_MOVE_MODEL                  "Move model"
#define TR_BACKUP_MODEL                "Backup model"
#define TR_DELETE_MODEL                "Delete model"
#define TR_RESTORE_MODEL               "Restore model"
#define TR_SDCARD_ERROR                TR("SD error", "SD card error")
#define TR_SDCARD                      "SD Card"
#define TR_NO_FILES_ON_SD              "No files on SD!"
#define TR_NO_SDCARD                   "No SD card"
#define TR_WAITING_FOR_RX              "Waiting for RX..."
#define TR_WAITING_FOR_TX              "Waiting for TX..."
#define TR_WAITING_FOR_MODULE          TR("Waiting module", "Waiting for module...")
#define TR_NO_TOOLS                    "No tools available"
#define TR_NORMAL                      "Normal"
#define TR_NOT_CONNECTED               TR("!Connected", "Not Connected")
#define TR_CONNECTED                   "Connected"
#define TR_FLEX_915                    "Flex 915MHz"
#define TR_FLEX_868                    "Flex 868MHz"
#define TR_16CH_WITHOUT_TELEMETRY      TR("16CH without telem.", "16CH without telemetry")
#define TR_16CH_WITH_TELEMETRY         TR("16CH with telem.", "16CH with telemetry")
#define TR_EXT_ANTENNA                 "Ext. antenna"
#define TR_PIN                         "Pin"
#define TR_UPDATE_TX_OPTIONS           "Update TX options?"
#define TR_MODULES_RX_VERSION          BUTTON("Modules / RX version")
#define TR_MENU_MODULES_RX_VERSION     "MODULES / RX VERSION"
#define TR_MENU_FIRM_OPTIONS           "FIRMWARE OPTIONS"
#define TR_IMU                        "IMU"
#define TR_STICKS_POTS_SLIDERS         "Axis/Pots/Sliders"
#define TR_RF_PROTOCOL "RF Protocol"
#define TR_POWER                       "Power"
#define TR_NO_TX_OPTIONS               "No TX options"
#define TR_RTC_BATT                    "RTC Batt"
#define TR_POWER_METER_EXT             "Power Meter (EXT)"
#define TR_POWER_METER_INT             "Power Meter (INT)"
#define TR_GPS_MODEL_LOCATOR           "GPS model locator"
#define TR_REFRESH                     "Refresh"
#define TR_SDCARD_FULL                 "SD card full"
#define TR_SDCARD_FULL_EXT              TR_BW_COL(TR_SDCARD_FULL "\036Logs & Screenshots" LCDW_128_LINEBREAK "disabled", TR_SDCARD_FULL "\nLogs and Screenshots disabled")
#define TR_NEEDS_FILE "NEEDS FILE"
#define TR_INCOMPATIBLE                "Incompatible"
#define TR_WARNING                     "WARNING"
#define TR_STORAGE_WARNING             "STORAGE"
#define TR_THROTTLE_UPPERCASE          "THROTTLE"
#define TR_ALARMSWARN                  "ALARMS"
#define TR_SWITCHWARN TR("SWITCH", "CONTROL")
#define TR_TEST_WARNING                TR("TESTING", "TEST BUILD")
#define TR_TEST_NOTSAFE                "Use for tests only"
#define TR_WARN_RTC_BATTERY_LOW "RTC Battery low"
#define TR_BATTERY                     "BATTERY"
#define TR_WRONG_PCBREV                "Wrong PCB detected"
#define TR_EMERGENCY_MODE "EMERGENCY MODE"
#define TR_KEYSTUCK                    "Key stuck"
#define TR_VOLUME                      "Volume"
#define TR_BRIGHTNESS                  "Brightness"
#define TR_CONTROL                     "Control"
#define TR_TTL_WARNING                 "Warning: Do not exceed 3.3V on TX/RX pins!"
#define TR_DURATION                    "Duration"
#define TR_NO_SOUNDS_ON_SD             "No sounds on SD"
#define TR_NO_MODELS_ON_SD             "No models on SD"
#define TR_NO_BITMAPS_ON_SD            "No bitmaps on SD"
#define TR_NO_SCRIPTS_ON_SD            "No scripts on SD"
#define TR_SCRIPT_SYNTAX_ERROR         TR("Syntax error", "Script syntax error")
#define TR_SCRIPT_PANIC                "Script panic"
#define TR_SCRIPT_ERROR                "Unknown error"
#define TR_PLAY_FILE                   "Play"
#define TR_DELETE_FILE                 "Delete"
#define TR_COPY_FILE                   "Copy"
#define TR_RENAME_FILE                 "Rename"
#define TR_ASSIGN_BITMAP               "Assign bitmap"
#define TR_EXECUTE_FILE                "Execute"
#define TR_REMOVED                     " removed"
#define TR_SD_INFO                     "Information"
#define TR_NA                          "N/A"
#define TR_TIME                        "Time"
#define TR_BAUDRATE                    "Baudrate"
#define TR_CRSF_ARMING_MODE            "Arm using"
#define TR_CRSF_ARMING_MODES           TR_CH"5", TR_SWITCH
#define TR_SAMPLE_MODE                 TR("Sampling","Sample Mode")
#define TR_SAMPLE_MODES_1              "Normal"
#define TR_SAMPLE_MODES_2              "OneBit"
#define TR_LOADING                     "Loading..."
#define TR_DELETE_THEME                "Delete Theme?"
#define TR_SAVE_THEME                  "Save Theme?"
#define TR_EDIT_COLOR                  "Edit Color"
#define TR_NO_THEME_IMAGE              "No theme image"
#define TR_BACKLIGHT_TIMER             "Inactivity timeout"

#define TR_MODEL_QUICK_SELECT        "Model quick select"
#define TR_LABELS_SELECT             "Label select"
#define TR_LABELS_MATCH              "Label matching"
#define TR_FAV_MATCH                 "Favorites matching"
#define TR_LABELS_SELECT_MODE_1      "Multi select"
#define TR_LABELS_SELECT_MODE_2      "Single select"
#define TR_LABELS_MATCH_MODE_1       "Match all"
#define TR_LABELS_MATCH_MODE_2       "Match any"
#define TR_FAV_MATCH_MODE_1          "Must match"
#define TR_FAV_MATCH_MODE_2          "Optional match"

#define TR_NO_TEMPLATES                "No model templates were found in this folder"
#define TR_SAVE_TEMPLATE               "Save as template"
#define TR_BLANK_MODEL                 "Blank Model"
#define TR_BLANK_MODEL_INFO            "Create a blank model"
#define TR_FILE_EXISTS                 "FILE ALREADY EXISTS"
#define TR_ASK_OVERWRITE               "Do you want to overwrite?"

#define TR_BLUETOOTH                   "Bluetooth"
#define TR_BLUETOOTH_INIT              "Init"
#define TR_BLUETOOTH_DIST_ADDR         "Dist addr"
#define TR_BLUETOOTH_LOCAL_ADDR        "Local addr"
#define TR_BLUETOOTH_PIN_CODE          "PIN code"
#define TR_BLUETOOTH_NODEVICES         "No Devices Found"
#define TR_BLUETOOTH_SCANNING          "Scanning..."
#define TR_BLUETOOTH_MODES_1           "---"
#define TR_BLUETOOTH_MODES_2           "Telemetry"
#define TR_BLUETOOTH_MODES_4           "Enabled"

#define TR_SD_INFO_TITLE               "SD INFO"
#define TR_SD_SECTORS                  "Sectors:"
#define TR_SD_SIZE                     "Size:"
#define TR_TYPE                        "Type"
#define TR_OWN                         "Own"
#define TR_DATE                        "Date"
#define TR_MONTHS_1                    "Jan"
#define TR_MONTHS_2                    "Feb"
#define TR_MONTHS_3                    "Mar"
#define TR_MONTHS_4                    "Apr"
#define TR_MONTHS_5                    "May"
#define TR_MONTHS_6                    "Jun"
#define TR_MONTHS_7                    "Jul"
#define TR_MONTHS_8                    "Aug"
#define TR_MONTHS_9                    "Sep"
#define TR_MONTHS_10                   "Oct"
#define TR_MONTHS_11                   "Nov"
#define TR_MONTHS_12                   "Dec"
#define TR_ROTARY_ENCODER              "R.E."
#define TR_ROTARY_ENC_MODE             TR("RotEnc Mode","Rotary Encoder Mode")
#define TR_CHANNELS_MONITOR            "CHANNELS MONITOR"
#define TR_PATH_TOO_LONG               "Path too long"
#define TR_VIEW_TEXT                   "View text"
#define TR_FLASH_BOOTLOADER            "Flash bootloader"
#define TR_FLASH_DEVICE                TR("Flash device","Flash device")
#define TR_FLASH_EXTERNAL_DEVICE TR("Flash S.Port", "Flash S.Port device")
#define TR_FLASH_BLUETOOTH_MODULE      TR("Flash BT module", "Flash Bluetooth module")
#define TR_DEVICE_NO_RESPONSE          TR("Device not responding", "Device not responding")
#define TR_DEVICE_FILE_ERROR           TR("Device file prob.", "Device file prob.")
#define TR_DEVICE_DATA_REFUSED         TR("Device data refused", "Device data refused")
#define TR_DEVICE_WRONG_REQUEST        TR("Device access problem", "Device access problem")
#define TR_DEVICE_FILE_REJECTED        TR("Device file refused", "Device file refused")
#define TR_DEVICE_FILE_WRONG_SIG       TR("Device file sig.", "Device file sig.")
#define TR_CURRENT_VERSION             TR("Curr Vers: ", "Current version: ")
#define TR_FLASH_INTERNAL_MODULE \
  TR("Flash int. module", "Flash internal module")
#define TR_FLASH_EXTERNAL_MODULE \
  TR("Flash ext. module", "Flash external module")
#define TR_FLASH_EXTERNAL_ELRS         TR("Flash Ext. ELRS", "Flash External ELRS")
#define TR_FIRMWARE_UPDATE_ERROR       TR("FW update error", "Firmware update error")
#define TR_FIRMWARE_UPDATE_SUCCESS     "Flash successful"
#define TR_WRITING                     "Writing..."
#define TR_INTERNALRF                  "Internal RF"
#define TR_INTERNAL_MODULE             TR("Int. module", "Internal module")
#define TR_EXTERNAL_MODULE             TR("Ext. module", "External module")
#define TR_EDGETX_UPGRADE_REQUIRED     "EdgeTX upgrade required"
#define TR_TELEMETRY_DISABLED          "Telem. disabled"
#define TR_MORE_OPTIONS_AVAILABLE      "More options available"
#define TR_EXTERNALRF "External RF"
#define TR_REG_ID                      "Reg. ID"
#define TR_OWNER_ID                    "Owner ID"
#define TR_HOLD                        "Hold"
#define TR_HOLD_UPPERCASE              "HOLD"
#define TR_NONE                        "None"
#define TR_NONE_UPPERCASE              "NONE"
#define TR_MENUSENSOR                  "SENSOR"
#define TR_MENUTOOLS                   "TOOLS"
#define TR_MIC_RECORDER                "Mic recorder"
#define TR_PUSH_TO_RECORD              "Push to record"
#define TR_RECORD                      "Record"
#define TR_STOP                        "Stop"
#define TR_REC                         "REC"
#define TR_STARTING_IN                 "Starting in"
#define TR_GET_READY                   "Get ready..."
#define TR_SAVED                       "Saved:"
#define TR_SAVE_AS                     "Save as"
#define TR_AUTO_TRIM                   "Auto-trim"
#define TR_TRIM_START                  "Trim start"
#define TR_TRIM_END                    "Trim end"
#define TR_OPEN_ERROR                  "Open error"
#define TR_TURN_OFF_RECEIVER           "Turn off receiver"
#define TR_STOPPING                    "Stopping..."
#define TR_MENU_POWER_METER            "POWER METER"
#define TR_SENSOR                      "SENSOR"
#define TR_COUNTRY_CODE                "Country code"
#define TR_USBMODE                     "USB mode"
#define TR_USB_CHARGE                  "Charge while radio on"
#define TR_VOICE_LANGUAGE              "Voice language"
#define TR_TEXT_LANGUAGE               "Text language"
#define TR_UNITS_SYSTEM                "Units"
#define TR_UNITS_PPM                   "Channel units"
#define TR_EDIT                        "Edit"
#define TR_INSERT_BEFORE               "Insert before"
#define TR_INSERT_AFTER                "Insert after"
#define TR_COPY                        "Copy"
#define TR_MOVE                        "Move"
#define TR_PASTE                       "Paste"
#define TR_PASTE_AFTER                 "Paste after"
#define TR_PASTE_BEFORE                "Paste before"
#define TR_DELETE                      "Delete"
#define TR_INSERT                      "Insert"
#define TR_RESET_SESSION               "Reset session"
#define TR_RESET_TIMER1                "Reset timer1"
#define TR_RESET_TIMER2                "Reset timer2"
#define TR_RESET_TIMER3                "Reset timer3"
#define TR_RESET_TELEMETRY             "Reset telemetry"
#define TR_STATISTICS                  "Statistics"
#define TR_ABOUT_US                    "About"
#define TR_USB_JOYSTICK                "USB Joystick (HID)"
#define TR_USB_MASS_STORAGE            "USB Storage (SD)"
#define TR_USB_SERIAL                  "USB Serial (VCP)"
#define TR_SF                          "SF"
#define TR_GF                          "GF"
#define TR_ANADIAGS_CALIB              "Calibrated analogs"
#define TR_ANADIAGS_FILTRAWDEV         "Filtered raw analogs with deviation"
#define TR_ANADIAGS_UNFILTRAW          "Unfiltered raw analogs"
#define TR_ANADIAGS_MINMAX             "Min., max. and range"
#define TR_ANADIAGS_MOVE               "Move analogs to their extremes!"
#define TR_BYTES                       "bytes"
#define TR_MODULE_BIND                 BUTTON(TR("Bnd", "Bind"))
#define TR_MODULE_UNBIND               BUTTON("Unbind")
#define TR_BT_SELECT_DEVICE            "Select device"
#define TR_DISCOVER                    BUTTON("Discover")
#define TR_BUTTON_INIT                 BUTTON("Init")
#define TR_WAITING "Waiting..."
#define TR_SHARE                       "Share"
#define TR_BIND                        "Bind"
#define TR_PAIRING                     "Pairing"
#define TR_BTAUDIO "BT Audio"
#define TR_MODULE_RANGE                BUTTON(TR("Rng", "Range"))
#define TR_RANGE_TEST "Range Test"
#define TR_RESET_BTN                   BUTTON("Reset")
#define TR_KEYS_BTN                    BUTTON("Keys")
#define TR_ANALOGS_BTN                 BUTTON(TR("Anas", "Analogs"))
#define TR_FS_BTN                      BUTTON(TR("Custom sw", TR_FUNCTION_SWITCHES))
#define TR_SET                         BUTTON("Set")
#define TR_CHANS                       "Chans"
#define TR_ANTENNAPROBLEM              "TX antenna problem!"
#define TR_MODELIDUSED                 "ID used in:"
#define TR_MODELIDUNIQUE               "ID is unique"
#define TR_MODULE                      "Module"
#define TR_TELEMETRY_TYPE              TR("Type", "Telemetry type")
#define TR_TELEMETRY_SENSORS           "Sensors"
#define TR_VALUE                       "Value"
#define TR_PERIOD                      "Period"
#define TR_INTERVAL                    "Interval"
#define TR_ENABLE                      "Enable"
#define TR_DISABLE                     "Disable"
#define TR_TOPLCDTIMER                 "Top LCD Timer"
#define TR_UNIT                        "Unit"
#define TR_TELEMETRY_NEWSENSOR         "Add new"
#define TR_CHANNELRANGE                TR("Ch. Range", "Channel Range")
#define TR_ANTENNACONFIRM1             "EXT. ANTENNA"
#define TR_ANTENNA_MODES_1           "Internal"
#define TR_ANTENNA_MODES_2           "Ask"
#define TR_ANTENNA_MODES_3           "Per model"
#define TR_ANTENNA_MODES_4           "Internal + External"
#define TR_ANTENNA_MODES_5           "External"
#define TR_USE_INTERNAL_ANTENNA        TR("Use int. antenna", "Use internal antenna")
#define TR_USE_EXTERNAL_ANTENNA        TR("Use ext. antenna", "Use external antenna")
#define TR_ANTENNACONFIRM2             TR("Check antenna", "Make sure antenna is installed!")
#define TR_MODULE_PROTOCOL_FLEX_WARN_LINE1   "Requires FLEX non"
#define TR_MODULE_PROTOCOL_FCC_WARN_LINE1    "Requires FCC"
#define TR_MODULE_PROTOCOL_EU_WARN_LINE1     "Requires EU"
#define TR_MODULE_PROTOCOL_WARN_LINE2        "certified firmware"
#define TR_LOWALARM                    "Low alarm"
#define TR_CRITICALALARM               "Critical alarm"
#define TR_DISABLE_ALARM               TR("Disable alarms", "Disable telemetry alarms")
#define TR_POPUP                       "Popup"
#define TR_MIN                         "Min"
#define TR_MAX                         "Max"
#define TR_MIRROR                      "Mirror"
#define TR_CLEAR                       "Clear"
#define TR_CLEAR_BTN                   BUTTON("Clear")
#define TR_RESET                       "Reset"
#define TR_RESET_SUBMENU               "Reset..."
#define TR_MIXSOURCE                   "Source (%)"
#define TR_PREFLIGHT_POTSLIDER_CHECK_1 "OFF"
#define TR_PREFLIGHT_POTSLIDER_CHECK_2 "ON"
#define TR_PREFLIGHT_POTSLIDER_CHECK_3 "AUTO"
#define TR_PREFLIGHT                   "Pre-start Checks"
#define TR_CHECKLIST                   TR("Checklist", "Display checklist")
#define TR_CHECKLIST_INTERACTIVE       TR3("C-Interact", "Interact. checklist", "Interactive checklist")
#define TR_AUX_SERIAL_MODE             "Serial port"
#define TR_AUX_SERIAL_PORT_POWER       "Port power"
#define TR_SCRIPT                      "Script"
#define TR_INPUTS                      "Inputs"
#define TR_NO_TELEMETRY_SCREENS        "No Telemetry Screens"
#define TR_TOUCH_PANEL                 "Touch panel:"
#define TR_FILE_SIZE                   "File size"
#define TR_FILE_OPEN                   "Open anyway?"

// Horus and Taranis column headers
#define TR_PHASES_HEADERS_NAME         "Name"
#define TR_PHASES_HEADERS_SW           "Switch"
#define TR_PHASES_HEADERS_FAD_IN       "Fade In"
#define TR_PHASES_HEADERS_FAD_OUT      "Fade Out"


// Horus layouts and widgets
#define TR_FIRST_CHANNEL             "First channel"
#define TR_LAST_CHANNEL              "Last channel"
#define TR_FILL_BACKGROUND           "Fill background?"
#define TR_BG_COLOR                  "BG Color"
#define TR_SLIDERS                   "Sliders"
#define TR_TIMER_SOURCE              "Timer source"
#define TR_SIZE                      "Size"
#define TR_SHADOW                    "Shadow"
#define TR_ALIGNMENT                 "Alignment"
#define TR_ALIGN_LABEL               "Align label"
#define TR_ALIGN_VALUE               "Align value"
#define TR_ALIGN_OPTS_1              "Left"
#define TR_ALIGN_OPTS_2              "Center"
#define TR_ALIGN_OPTS_3              "Right"
#define TR_TEXT                      "Text"
#define TR_COLOR                     "Color"
#define TR_PANEL1_BACKGROUND         "Panel1 background"
#define TR_PANEL2_BACKGROUND         "Panel2 background"
#define TR_PANEL_BACKGROUND          "Background"
#define TR_PANEL_COLOR               "  Color"
#define TR_WIDGET_GAUGE              "Gauge"
#define TR_WIDGET_MODELBMP           "Model info"
#define TR_WIDGET_OUTPUTS            "Outputs"
#define TR_WIDGET_TEXT               "Text"
#define TR_WIDGET_TIMER              "Timer"
#define TR_WIDGET_VALUE              "Value"

// About screen
#define TR_ABOUTUS                     TR(" ABOUT ", "ABOUT")

#define TR_BEEP_VOLUME                 "Beep volume"
#define TR_WAV_VOLUME                  "Wav volume"
#define TR_BG_VOLUME                   TR("Bg volume", "Background volume")

#define TR_TOP_BAR                     "Top bar"
#define TR_FLASH_ERASE                 "Flash erase..."
#define TR_FLASH_WRITE "Flash write..."
#define TR_MODULE_RESET                "Module reset..."
#define TR_UNSUPPORTED_RX "Unsupported RX"
#define TR_DEVICE_RESET                "Device reset..."
#define TR_ALTITUDE                    "Altitude"
#define TR_SCALE                       "Scale"
#define TR_VIEW_CHANNELS               "View Channels"
#define TR_VIEW_NOTES                  "View Notes"
#define TR_ID                          "ID"
#define TR_PRECISION                   "Precision"
#define TR_RATIO                       "Ratio"
#define TR_FORMULA                     "Formula"
#define TR_CELLINDEX                   "Cell index"
#define TR_LOGS                        "Logs"
#define TR_OPTIONS                     "Options"
#define TR_FIRMWARE_OPTIONS            BUTTON("Firmware options")

#define TR_ALTSENSOR                   "Alt sensor"
#define TR_CELLSENSOR                  "Cell sensor"
#define TR_GPSSENSOR                   "GPS sensor"
#define TR_GYRO                        "Gyro"
#define TR_CURRENTSENSOR               "Sensor"
#define TR_AUTOOFFSET                  "Auto Offset"
#define TR_ONLYPOSITIVE                "Positive"
#define TR_FILTER                      "Filter"
#define TR_TELEMETRYFULL               TR("All slots full!", "All telemetry slots full!")
#define TR_IGNORE_INSTANCE             TR("No inst.", "Ignore instances")
#define TR_SHOW_INSTANCE_ID            "Show instance ID"
#define TR_DISCOVER_SENSORS            "Discover new"
#define TR_STOP_DISCOVER_SENSORS       "Stop"
#define TR_DELETE_ALL_SENSORS          "Delete all"
#define TR_CONFIRMDELETE               "Really " LCDW_128_LINEBREAK "delete all ?"
#define TR_SELECT_WIDGET               "Select widget"
#define TR_WIDGET_FULLSCREEN           "Full screen"
#define TR_REMOVE_WIDGET               "Remove widget"
#define TR_WIDGET_SETTINGS             "Widget settings"
#define TR_REMOVE_SCREEN               "Remove screen"
#define TR_SETUP_WIDGETS               "Setup widgets"
#define TR_THEME                       "Theme"
#define TR_LAYOUT                      "Layout"
#define TR_TEXT_COLOR                  "Text color"
#define TR_MENU_STICKS                 CHAR_STICK "Axis"
#define TR_MENU_POTS                   CHAR_POT "Pots"
#define TR_MENU_MIN                    CHAR_FUNCTION "MIN"
#define TR_MENU_MAX                    CHAR_FUNCTION "MAX"
#define TR_MENU_TRIMS                  CHAR_TRIM "Trims"
#define TR_MENU_SWITCHES               CHAR_SWITCH "Switches"
#define TR_MENU_CHANNELS               CHAR_CHANNEL "Channels"
#define TR_MENU_TELEMETRY              CHAR_TELEMETRY "Telemetry"
#define TR_MENU_DISPLAY                "DISPLAY"
#define TR_MENU_OTHER                  "Other"
#define TR_MENU_INVERT                 "Invert"
#define TR_AUDIO_MUTE                  TR("Audio mute","Mute if no sound")
#define TR_PWM_OUTPUT                  "PWM output"
#define TR_JITTER_FILTER               "ADC filter"
#define TR_DEAD_ZONE                   "Dead zone"
#define TR_RTC_CHECK                   TR("Check RTC", "Check RTC voltage")
#define TR_AUTH_FAILURE                "Auth-failure"
#define TR_RACING_MODE                 "Racing mode"

#define TR_USE_THEME_COLOR              "Use theme color"

#define TR_DUPLICATE                    "Duplicate"
#define TR_ACTIVATE                     "Set Active"
#define TR_COLOR_PICKER                 "Color Picker"
#define TR_FIXED                        "Fixed"
#define TR_EDIT_THEME_DETAILS           "Edit Theme Details"
#define TR_THEME_COLOR_PRIMARY1        "PRIMARY1"
#define TR_THEME_COLOR_PRIMARY2        "PRIMARY2"
#define TR_THEME_COLOR_PRIMARY3        "PRIMARY3"
#define TR_THEME_COLOR_SECONDARY1      "SECONDARY1"
#define TR_THEME_COLOR_SECONDARY2      "SECONDARY2"
#define TR_THEME_COLOR_SECONDARY3      "SECONDARY3"
#define TR_THEME_COLOR_FOCUS           "FOCUS"
#define TR_THEME_COLOR_EDIT            "EDIT"
#define TR_THEME_COLOR_ACTIVE          "ACTIVE"
#define TR_THEME_COLOR_WARNING         "WARNING"
#define TR_THEME_COLOR_DISABLED        "DISABLED"
#define TR_THEME_COLOR_QM_BG           "Quick Menu BG"
#define TR_THEME_COLOR_QM_FG           "Quick Menu FG"
#define TR_THEME_COLOR_CUSTOM          "CUSTOM"
#define TR_THEME_CHECKBOX              "Checkbox"
#define TR_THEME_ACTIVE                "Active"
#define TR_THEME_REGULAR               "Regular"
#define TR_THEME_WARNING               "Warning"
#define TR_THEME_DISABLED              "Disabled"
#define TR_THEME_EDIT                  "Edit"
#define TR_THEME_FOCUS                 "Focus"
#define TR_AUTHOR                       "Author"
#define TR_DESCRIPTION                  "Description"
#define TR_SAVE                         "Save"
#define TR_CANCEL                       "Cancel"
#define TR_EDIT_THEME                   "EDIT THEME"
#define TR_DETAILS                      "Details"

// Voice in native language
#define TR_VOICE_ENGLISH                "English"
#define TR_VOICE_CHINESE                "Chinese"
#define TR_VOICE_CZECH                  "Czech"
#define TR_VOICE_DANISH                 "Danish"
#define TR_VOICE_DEUTSCH                "German"
#define TR_VOICE_DUTCH                  "Dutch"
#define TR_VOICE_ESPANOL                "Spanish"
#define TR_VOICE_FINNISH                "Finnish"
#define TR_VOICE_FRANCAIS               "French"
#define TR_VOICE_HUNGARIAN              "Hungarian"
#define TR_VOICE_ITALIANO               "Italian"
#define TR_VOICE_POLISH                 "Polish"
#define TR_VOICE_PORTUGUES              "Portuguese"
#define TR_VOICE_RUSSIAN                "Russian"
#define TR_VOICE_SLOVAK                 "Slovak"
#define TR_VOICE_SWEDISH                "Swedish"
#define TR_VOICE_TAIWANESE              "Taiwanese"
#define TR_VOICE_JAPANESE               "Japanese"
#define TR_VOICE_HEBREW                 "Hebrew"
#define TR_VOICE_UKRAINIAN              "Ukrainian"
#define TR_VOICE_KOREAN                 "Korean"

#define TR_USBJOYSTICK_LABEL           "USB Joystick"
#define TR_USBJOYSTICK_EXTMODE         "Mode"
#define TR_VUSBJOYSTICK_EXTMODE_1      "Classic"
#define TR_VUSBJOYSTICK_EXTMODE_2      "Advanced"
#define TR_USBJOYSTICK_SETTINGS        BUTTON("Channel Settings")
#define TR_USBJOYSTICK_IF_MODE         TR("If. mode","Interface mode")
#define TR_VUSBJOYSTICK_IF_MODE_1      "Joystick"
#define TR_VUSBJOYSTICK_IF_MODE_2      "Gamepad"
#define TR_VUSBJOYSTICK_IF_MODE_3      "MultiAxis"
#define TR_USBJOYSTICK_CH_MODE         "Mode"
#define TR_VUSBJOYSTICK_CH_MODE_1      "None"
#define TR_VUSBJOYSTICK_CH_MODE_2      "Btn"
#define TR_VUSBJOYSTICK_CH_MODE_3      "Axis"
#define TR_VUSBJOYSTICK_CH_MODE_4      "Sim"
#define TR_VUSBJOYSTICK_CH_MODE_S_1    "-"
#define TR_VUSBJOYSTICK_CH_MODE_S_2    "B"
#define TR_VUSBJOYSTICK_CH_MODE_S_3    "A"
#define TR_VUSBJOYSTICK_CH_MODE_S_4    "S"
#define TR_USBJOYSTICK_CH_BTNMODE      "Button Mode"
#define TR_VUSBJOYSTICK_CH_BTNMODE_1   "Normal"
#define TR_VUSBJOYSTICK_CH_BTNMODE_2   "Pulse"
#define TR_VUSBJOYSTICK_CH_BTNMODE_3   "SWEmu"
#define TR_VUSBJOYSTICK_CH_BTNMODE_4   "Delta"
#define TR_VUSBJOYSTICK_CH_BTNMODE_5   "Companion"
#define TR_VUSBJOYSTICK_CH_BTNMODE_S_1 TR("Norm","Normal")
#define TR_VUSBJOYSTICK_CH_BTNMODE_S_2 TR("Puls","Pulse")
#define TR_VUSBJOYSTICK_CH_BTNMODE_S_3 TR("SWEm","SWEmul")
#define TR_VUSBJOYSTICK_CH_BTNMODE_S_4 TR("Delt","Delta")
#define TR_VUSBJOYSTICK_CH_BTNMODE_S_5 TR("CPN","Companion")
#define TR_USBJOYSTICK_CH_SWPOS        "Positions"
#define TR_VUSBJOYSTICK_CH_SWPOS_1     "Push"
#define TR_VUSBJOYSTICK_CH_SWPOS_2     "2POS"
#define TR_VUSBJOYSTICK_CH_SWPOS_3     "3POS"
#define TR_VUSBJOYSTICK_CH_SWPOS_4     "4POS"
#define TR_VUSBJOYSTICK_CH_SWPOS_5     "5POS"
#define TR_VUSBJOYSTICK_CH_SWPOS_6     "6POS"
#define TR_VUSBJOYSTICK_CH_SWPOS_7     "7POS"
#define TR_VUSBJOYSTICK_CH_SWPOS_8     "8POS"
#define TR_USBJOYSTICK_CH_AXIS         "Axis"
#define TR_VUSBJOYSTICK_CH_AXIS_1      "X"
#define TR_VUSBJOYSTICK_CH_AXIS_2      "Y"
#define TR_VUSBJOYSTICK_CH_AXIS_3      "Z"
#define TR_VUSBJOYSTICK_CH_AXIS_4      "rotX"
#define TR_VUSBJOYSTICK_CH_AXIS_5      "rotY"
#define TR_VUSBJOYSTICK_CH_AXIS_6      "rotZ"
#define TR_VUSBJOYSTICK_CH_AXIS_7      "Slider"
#define TR_VUSBJOYSTICK_CH_AXIS_8      "Dial"
#define TR_VUSBJOYSTICK_CH_AXIS_9      "Wheel"
#define TR_USBJOYSTICK_CH_SIM          "Sim axis"
#define TR_VUSBJOYSTICK_CH_SIM_1       "Ail"
#define TR_VUSBJOYSTICK_CH_SIM_2       "Ele"
#define TR_VUSBJOYSTICK_CH_SIM_3       "Rud"
#define TR_VUSBJOYSTICK_CH_SIM_4       "Thr"
#define TR_VUSBJOYSTICK_CH_SIM_5       "Acc"
#define TR_VUSBJOYSTICK_CH_SIM_6       "Brk"
#define TR_VUSBJOYSTICK_CH_SIM_7       "Steer"
#define TR_VUSBJOYSTICK_CH_SIM_8       "Dpad"
#define TR_USBJOYSTICK_CH_INVERSION    "Inversion"
#define TR_USBJOYSTICK_CH_BTNNUM       "Button no."
#define TR_USBJOYSTICK_BTN_COLLISION   "!Button no. collision!"
#define TR_USBJOYSTICK_AXIS_COLLISION  "!Axis collision!"
#define TR_USBJOYSTICK_CIRC_COUTOUT    TR("Circ. cut", "Circular cutout")
#define TR_VUSBJOYSTICK_CIRC_COUTOUT_1 "None"
#define TR_VUSBJOYSTICK_CIRC_COUTOUT_2 "X-Y, Z-rX"
#define TR_VUSBJOYSTICK_CIRC_COUTOUT_3 "X-Y, rX-rY"
#define TR_VUSBJOYSTICK_CIRC_COUTOUT_4 "X-Y, Z-rZ"
#define TR_USBJOYSTICK_APPLY_CHANGES   BUTTON("Apply changes")

#define TR_DIGITAL_SERVO          "Servo333HZ"
#define TR_ANALOG_SERVO           "Servo 50HZ"
#define TR_SIGNAL_OUTPUT          "Signal output"
#define TR_SERIAL_BUS             "Serial bus"
#define TR_SYNC                   "Sync"

#define TR_ENABLED_FEATURES       "Enabled Features"
#define TR_RADIO_MENU_TABS        "Radio Menu"
#define TR_MODEL_MENU_TABS        "Model Menu"

#define TR_SELECT_MENU_ALL        "All"
#define TR_SELECT_MENU_CLR        "Clear"
#define TR_SELECT_MENU_INV        "Invert"

#define TR_SORT_ORDERS_1          "Name A-Z"
#define TR_SORT_ORDERS_2          "Name Z-A"
#define TR_SORT_ORDERS_3          "Least used"
#define TR_SORT_ORDERS_4          "Most used"
#define TR_SORT_MODELS_BY         "Sort models by"
#define TR_CREATE_NEW             "Create"

#define TR_THEME_EXISTS           "A theme directory with the same name already exists."

#define TR_DATE_TIME_WIDGET       "Date & Time"
#define TR_RADIO_INFO_WIDGET      "Radio Info"
#define TR_LOW_BATT_COLOR         "Low battery"
#define TR_MID_BATT_COLOR         "Mid battery"
#define TR_HIGH_BATT_COLOR        "High battery"

#define TR_WIDGET_SIZE            "Widget size"

#define TR_DEL_DIR_NOT_EMPTY      "Directory must be empty before deletion"

#define TR_KEY_SHORTCUTS          "Key Shortcuts"
#define TR_CURRENT_SCREEN         "Current Screen"
#define TR_SHORT_PRESS            "Short Press"
#define TR_LONG_PRESS             "Long Press"
#define TR_OPEN_QUICK_MENU        "Open Quick Menu"
#define TR_QUICK_MENU_FAVORITES   "Quick Menu Favorites"

#define TR_LABEL_MATCH_ALL "AND"
#define TR_LABEL_MATCH_ANY "OR"

#define TR_UPDATE_LIST "[update list]"

#define TR_TIMER_DIRECTION             "Direction"

#define TR_CHANNEL_MAPPING "Channel mapping"
