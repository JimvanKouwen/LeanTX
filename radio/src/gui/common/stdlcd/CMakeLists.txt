set(GUI_SRC
  ${GUI_SRC}
  ../common/stdlcd/menu_model.cpp
  ../common/stdlcd/menu_radio.cpp
  ../common/stdlcd/menus_common.cpp
  ../common/stdlcd/draw_functions.cpp
  ../common/stdlcd/fonts.cpp
  ../common/stdlcd/utf8.cpp
  ../common/stdlcd/popups_common.cpp
  ../common/stdlcd/model_channel_mappings.cpp
  ../common/stdlcd/view_text.cpp
  ../common/stdlcd/model_notes.cpp
  ../common/stdlcd/radio_hardware.cpp
  ../common/stdlcd/radio_version.cpp
  ../common/stdlcd/view_telemetry.cpp
  ../common/stdlcd/calibration.cpp
  ../common/stdlcd/splash.cpp
  ../common/stdlcd/radio_sdmanager.cpp
  ../common/stdlcd/widgets_common.cpp
  ../common/stdlcd/lcd_common.cpp
  )

if(LUA)
  set(GUI_SRC ${GUI_SRC} ../common/stdlcd/radio_tools.cpp)
endif()

if(USBJ_EX)
  set(GUI_SRC
    ${GUI_SRC}
    ../common/stdlcd/model_usbjoystick.cpp
    )
endif()
