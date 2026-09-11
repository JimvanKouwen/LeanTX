
find_package(SDL2 QUIET COMPONENTS SDL2 CONFIG)
if(TARGET SDL2::SDL2)
  message(STATUS "SDL2 found")
else()
  message(STATUS "SDL not found! Simulator audio, and joystick inputs, will not work.")
endif()

# Windows-specific includes and libs shared by sub-projects
if(WIN32)
  # TODO: is that still necessary?
  set(CMAKE_C_USE_RESPONSE_FILE_FOR_INCLUDES OFF)
  set(CMAKE_C_USE_RESPONSE_FILE_FOR_LIBRARIES OFF)
  set(CMAKE_CXX_USE_RESPONSE_FILE_FOR_INCLUDES OFF)
  set(CMAKE_CXX_USE_RESPONSE_FILE_FOR_LIBRARIES OFF)
endif()

# google tests
include(FetchGtest)

add_custom_target(tests-radio
  COMMAND ${CMAKE_CURRENT_BINARY_DIR}/gtests-radio
  DEPENDS gtests-radio
)

add_custom_target(gtests
  DEPENDS gtests-radio
)
add_custom_target(tests
  DEPENDS tests-radio
)

set(IGNORE "${ARM_TOOLCHAIN_DIR}")
