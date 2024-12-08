
cmake_minimum_required(VERSION 3.13...3.27)

include(pico-sdk/pico_sdk_init.cmake)

pico_sdk_init()

project(MyButtons)

enable_language(C CXX ASM)

set(CMAKE_CXX_STANDARD 26)

include(FetchContent)
include(cmake/findFmt.cmake)
include(cmake/findCgui.cmake)

add_subdirectory(pico-linux-libc)
add_subdirectory(cpp-test-anywhere)
add_subdirectory(inc)

set(EXE_NAME my_buttons)
add_executable(${EXE_NAME} src/my_buttons.cpp)

# pull in common dependencies
target_link_libraries(${EXE_NAME} PUBLIC dooc::picolinuxc pico_stdlib fmt::fmt myb::myb_headers)

# enable usb output, disable uart output
pico_enable_stdio_usb(${EXE_NAME} 1)
pico_enable_stdio_uart(${EXE_NAME} 0)

# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(${EXE_NAME})


set(EXE_NAME my_buttons_tests)
add_executable(${EXE_NAME} src/my_buttons_tests.cpp)

# pull in common dependencies
target_link_libraries(${EXE_NAME} PUBLIC dooc::picolinuxc pico_stdlib fmt::fmt cta::cta myb::myb_headers)
cta_add_test(${EXE_NAME})

# enable usb output, disable uart output
pico_enable_stdio_usb(${EXE_NAME} 1)
pico_enable_stdio_uart(${EXE_NAME} 0)

# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(${EXE_NAME})

