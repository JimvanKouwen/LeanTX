set(GUI_SRC
  ${GUI_SRC}
  lcd.cpp
  splash.cpp
  popups.cpp
  widgets.cpp
  startup_shutdown.cpp
  model_select.cpp
  model_setup.cpp
  model_outputs.cpp
  model_telemetry.cpp
  model_telemetry_sensor.cpp
  radio_setup.cpp
  radio_calibration.cpp
  view_main.cpp
  view_statistics.cpp
  )


include(gui/common/stdlcd/CMakeLists.txt)

set(BOOTLOADER_SRC ${BOOTLOADER_SRC}
  ${RADIO_SRC_DIR}/gui/212x64/lcd.cpp
  ${RADIO_SRC_DIR}/gui/common/stdlcd/lcd_common.cpp
  ${RADIO_SRC_DIR}/gui/common/stdlcd/fonts.cpp
  ${RADIO_SRC_DIR}/gui/common/stdlcd/utf8.cpp
)
