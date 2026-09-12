set(TIMERS 3 CACHE STRING "Timers count (2 or 3)")
set_property(CACHE TIMERS PROPERTY STRINGS 2 3)
option(CLI "Command Line Interface" OFF)
option(ENABLE_SERIAL_PASSTHROUGH "Enable serial passthrough" OFF)
option(DEBUG "Debug mode" OFF)
option(LOG_TELEMETRY "Telemetry Logs on SD card" OFF)
option(LOG_BLUETOOTH "Bluetooth Logs on SD card" OFF)
option(TRACE_SD_CARD "Traces SD enabled" OFF)
option(TRACE_FATFS "Traces FatFS enabled" OFF)
option(DEBUG_TRACE_BUFFER "Debug Trace Screen" OFF)
# CRSF is the only RF module protocol (ExpressLRS and TBS Crossfire).
set(CROSSFIRE YES)
option(DEBUG_INTERRUPTS "Count interrupts" OFF)
option(DEBUG_LATENCY "Debug latency" OFF)
option(DEBUG_USB_INTERRUPTS "Count individual USB interrupts" OFF)
option(DEBUG_TIMERS "Time critical parts of the code" OFF)
option(DEBUG_BLUETOOTH "Debug Bluetooth" OFF)

if("CRSF" IN_LIST INTERNAL_MODULES)
  option(INTERNAL_MODULE_CRSF "Support for CRSF internal module" ON)
  if(INTERNAL_MODULE_CRSF)
    add_definitions(-DHARDWARE_INTERNAL_MODULE -DINTERNAL_MODULE_CRSF)
  endif()
else()
  set(INTERNAL_MODULE_CRSF OFF)
endif()

# Existing build caches and radios shipped with legacy RF must default to off.
if(NOT INTERNAL_MODULE_CRSF OR NOT DEFAULT_INTERNAL_MODULE STREQUAL "CROSSFIRE")
  set(DEFAULT_INTERNAL_MODULE NONE)
endif()
add_definitions(-DDEFAULT_INTERNAL_MODULE=MODULE_TYPE_${DEFAULT_INTERNAL_MODULE})

if(MODULE_SIZE_STD)
  add_definitions(-DHARDWARE_EXTERNAL_MODULE_SIZE_STD)
endif()

if(MODULE_SIZE_SML)
  add_definitions(-DHARDWARE_EXTERNAL_MODULE_SIZE_SML)
endif()

if(NOT MODULE_SIZE_STD AND NOT MODULE_SIZE_SML)
  set(HARDWARE_EXTERNAL_MODULE NO)
endif()

if(HARDWARE_EXTERNAL_MODULE)
  add_definitions(-DHARDWARE_EXTERNAL_MODULE)
endif()

if(TIMERS EQUAL 3)
  add_definitions(-DTIMERS=3)
else()
  add_definitions(-DTIMERS=2)
endif()

if(LOG_TELEMETRY)
  add_definitions(-DLOG_TELEMETRY)
endif()

if(LOG_BLUETOOTH)
  add_definitions(-DLOG_BLUETOOTH)
endif()

if(TRACE_SD_CARD)
  add_definitions(-DTRACE_SD_CARD)
  set(DEBUG ON)
  set(DEBUG_TRACE_BUFFER ON)
endif()

if(TRACE_FATFS)
  add_definitions(-DTRACE_FATFS)
  set(DEBUG ON)
  set(DEBUG_TRACE_BUFFER ON)
endif()

if(DEBUG_TRACE_BUFFER)
  add_definitions(-DDEBUG_TRACE_BUFFER)
endif()

if(DEBUG_INTERRUPTS)
  add_definitions(-DDEBUG_INTERRUPTS)
  set(DEBUG ON)
  if(DEBUG_USB_INTERRUPTS)
    add_definitions(-DDEBUG_USB_INTERRUPTS)
  endif()
endif()

if(DEBUG_TIMERS)
  add_definitions(-DDEBUG_TIMERS)
  set(DEBUG ON)
endif()

if(DEBUG_LATENCY STREQUAL MIXER_RF)
  add_definitions(-DDEBUG_LATENCY)
  add_definitions(-DDEBUG_LATENCY_MIXER_RF)
endif()

if(DEBUG_LATENCY STREQUAL RF_ONLY)
  add_definitions(-DDEBUG_LATENCY)
  add_definitions(-DDEBUG_LATENCY_RF_ONLY)
endif()

if(DEBUG_LATENCY STREQUAL END_TO_END)
  add_definitions(-DDEBUG_LATENCY)
  add_definitions(-DDEBUG_LATENCY_END_TO_END)
endif()

if(DEBUG_BLUETOOTH)
  add_definitions(-DDEBUG_BLUETOOTH)
  option(DEBUG_BLUETOOTH_VERBOSE "Debug Bluetooth Verbose" OFF)
  if(DEBUG_BLUETOOTH_VERBOSE)
    add_definitions(-DDEBUG_BLUETOOTH_VERBOSE)
  endif()
endif()

if(CLI)
  add_definitions(-DCLI)
  set(FIRMWARE_SRC ${FIRMWARE_SRC} cli.cpp)
endif()

if(CLI OR DEBUG)
  set(SRC ${SRC} dump.cpp)
endif()

if(ENABLE_SERIAL_PASSTHROUGH)
  add_definitions(-DENABLE_SERIAL_PASSTHROUGH)
endif()

if(HAPTIC)
  add_definitions(-DHAPTIC)
  set(SRC ${SRC} haptic.cpp)
endif()

add_definitions(-DCROSSFIRE)
set(PULSES_SRC ${PULSES_SRC} crossfire.cpp)
set(SRC ${SRC} telemetry/crossfire.cpp io/elrs_firmware_update.cpp)
if(BLUETOOTH)
  set(SRC ${SRC} io/bluetooth_firmware_container.cpp)
endif()

add_definitions(-g)

add_definitions(-DCPUARM)
add_definitions(-DGPS)
add_definitions(-DBATTGRAPH -DTHRTRACE)

foreach(LANGUAGE ${TTS_LANGUAGES})
  string(TOLOWER ${LANGUAGE} lang_lower)
  set(SRC ${SRC} translations/tts/tts_${lang_lower}.cpp)
endforeach()

set(SRC
  ${SRC}
  main.cpp
  tasks.cpp
  telemetry/telemetry.cpp
  telemetry/telemetry_sensors.cpp
  crc.cpp
  vario.cpp
  )

set(FIRMWARE_TARGET_SRC
  ${FIRMWARE_TARGET_SRC}
  # keys_driver.cpp
  # telemetry_driver.cpp
)

set(PULSES_SRC
  ${PULSES_SRC}
  pulses.cpp
  ppm.cpp
  modules_helpers.cpp
  )
