if(HARDWARE_KEYS)
  add_definitions(-DHARDWARE_KEYS)
endif()

if(HARWARE_TOUCH)
  add_definitions(-DHARDWARE_TOUCH)
endif()

include_directories(
  ${RADIO_SRC_DIR}/gui/colorlcd
  ${RADIO_SRC_DIR}/gui/colorlcd/layouts
  ${RADIO_SRC_DIR}/gui/colorlcd/libui
  ${RADIO_SRC_DIR}/gui/colorlcd/model
  ${RADIO_SRC_DIR}/gui/colorlcd/radio
  ${RADIO_SRC_DIR}/gui/colorlcd/module
  ${RADIO_SRC_DIR}/gui/colorlcd/mainview
  ${RADIO_SRC_DIR}/gui/colorlcd/setup_menus
  ${RADIO_SRC_DIR}/gui/colorlcd/themes
  ${RADIO_SRC_DIR}/gui/colorlcd/controls
)

file(GLOB THEMES_SRC RELATIVE ${RADIO_SRC_DIR}/gui/colorlcd
  ${RADIO_SRC_DIR}/gui/colorlcd/themes/*.cpp)

file(GLOB LAYOUTS_SRC RELATIVE ${RADIO_SRC_DIR}/gui/colorlcd
  ${RADIO_SRC_DIR}/gui/colorlcd/layouts/*.cpp)

file(GLOB WIDGETS_SRC RELATIVE ${RADIO_SRC_DIR}/gui/colorlcd
  ${RADIO_SRC_DIR}/gui/colorlcd/widgets/*.cpp)

file(GLOB LIBUI_SRC RELATIVE ${RADIO_SRC_DIR}/gui/colorlcd
  ${RADIO_SRC_DIR}/gui/colorlcd/libui/*.cpp)

file(GLOB MAINVIEW_SRC CONFIGURE_DEPENDS RELATIVE ${RADIO_SRC_DIR}/gui/colorlcd
  ${RADIO_SRC_DIR}/gui/colorlcd/mainview/*.cpp)

file(GLOB MENUS_SRC RELATIVE ${RADIO_SRC_DIR}/gui/colorlcd
  ${RADIO_SRC_DIR}/gui/colorlcd/setup_menus/*.cpp)

file(GLOB CONTROLS_SRC CONFIGURE_DEPENDS RELATIVE ${RADIO_SRC_DIR}/gui/colorlcd
  ${RADIO_SRC_DIR}/gui/colorlcd/controls/*.cpp)

set(GUI_SRC
  ${GUI_SRC}
  ${LIBUI_SRC}
  ${THEMES_SRC}
  ${LAYOUTS_SRC}
  ${WIDGETS_SRC}
  ${MAINVIEW_SRC}
  ${MENUS_SRC}
  ${CONTROLS_SRC}
  bitmaps.cpp
  colors.cpp
  fonts.cpp
  lcd.cpp
  LvglWrapper.cpp
  startup_shutdown.cpp

  model/curveedit.cpp
  model/input_edit.cpp
  model/mixer_edit.cpp
  model/model_curves.cpp
  model/model_inputs.cpp
  model/model_mixes.cpp
  model/model_outputs.cpp
  model/model_select.cpp
  model/model_setup.cpp
  model/model_telemetry.cpp
  model/model_templates.cpp
  model/output_edit.cpp
  model/preflight_checks.cpp
  model/timer_setup.cpp

  module/module_setup.cpp

  radio/hw_extmodule.cpp
  radio/hw_inputs.cpp
  radio/hw_intmodule.cpp
  radio/hw_serial.cpp
  radio/preview_window.cpp
  radio/radio_calibration.cpp
  radio/radio_diaganas.cpp
  radio/radio_diagkeys.cpp
  radio/radio_hardware.cpp
  radio/radio_sdmanager.cpp
  radio/radio_setup.cpp
  radio/radio_theme.cpp
  radio/radio_tools.cpp
  radio/radio_version.cpp
  radio/radio_gps_tool.cpp
  radio/radio_mic_recorder.cpp
  )

macro(add_gui_src src)
  set(GUI_SRC
    ${GUI_SRC}
    ${src}
    )
endmacro(add_gui_src)

if(LUA)
  add_gui_src(standalone_lua.cpp)
  set(SRC ${SRC}
    lua/lua_widget.cpp
    lua/lua_widget_factory.cpp
    lua/widgets.cpp
    lua/lua_lvgl_widget.cpp
    )
endif()

if(LUA_MIXER)
  add_gui_src(model/model_mixer_scripts.cpp)
endif()

if(FUNCTION_SWITCHES)
  add_gui_src(model/function_switches.cpp)
  add_gui_src(radio/radio_cfs.cpp)
  add_gui_src(radio/radio_diagcustswitches.cpp)
endif()

if(CROSSFIRE)
  add_gui_src(module/crossfire_settings.cpp)
endif()

if(EXTERNAL_ANTENNA)
  add_gui_src(module/ext_antenna_settings.cpp)
endif()

if(BLUETOOTH)
  add_gui_src(radio/hw_bluetooth.cpp)
endif()

if(USBJ_EX)
  add_gui_src(model/model_usbjoystick.cpp)
endif()

option(UI_PERF_MONITOR "Draw frame rate and CPU usage" OFF)
if(UI_PERF_MONITOR)
  add_definitions(-DUI_PERF_MONITOR)
endif()

# include lvgl
include(${RADIO_SRC_DIR}/gui/colorlcd/CMakeListsLVGL.txt)

list(TRANSFORM LVGL_SOURCES_MINIMAL PREPEND ${RADIO_SRC_DIR}/)
list(TRANSFORM LVGL_FONT_SOURCES_MINIMAL PREPEND ${RADIO_SRC_DIR}/)

set(BOOTLOADER_SRC ${BOOTLOADER_SRC}
  ${LVGL_SOURCES_MINIMAL}
  ${LVGL_FONT_SOURCES_MINIMAL}
  ${RADIO_SRC_DIR}/gui/colorlcd/boot_lcd.cpp
  ${RADIO_SRC_DIR}/gui/colorlcd/fonts.cpp
  ${RADIO_SRC_DIR}/gui/colorlcd/colors.cpp
  ${RADIO_SRC_DIR}/gui/colorlcd/bitmaps.cpp
  ${RADIO_SRC_DIR}/gui/colorlcd/libui/bitmapbuffer.cpp
  ${RADIO_SRC_DIR}/thirdparty/lz4/lz4.c
)

set(SRC ${SRC}
  ${LVGL_SOURCES}
  ${LVGL_FONT_SOURCES}
  ${RADIO_SRC_DIR}/thirdparty/lz4/lz4.c
)

add_definitions(-DCOLORLCD)
