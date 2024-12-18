
cmake_minimum_required(VERSION 3.13...3.27)

set(MYB_RPI_PICO ON CACHE BOOL "")

if (MYB_RPI_PICO)
    include(pico-sdk/pico_sdk_init.cmake)
    pico_sdk_init()
endif()

project(MyButtons)

enable_language(C CXX ASM)

set(CMAKE_CXX_STANDARD 26)

include(FetchContent)
include(cmake/findFmt.cmake)
include(cmake/findCgui.cmake)

add_subdirectory(cpp-test-anywhere)
add_subdirectory(inc)

set(TEST_NAME my_buttons_tests)
add_executable(${TEST_NAME} src/my_buttons_tests.cpp)

enable_testing()
cta_add_test(${TEST_NAME})

if (MYB_RPI_PICO)
    add_subdirectory(pico-linux-libc)
    set(MYB_EXTRA_LINKS dooc::picolinuxc pico_stdlib)
    function (myb_add_app NAME SRC)
        add_executable(${NAME} ${SRC})
        # disable usb output, disable uart output
        pico_enable_stdio_usb(${NAME} 0)
        pico_enable_stdio_uart(${NAME} 0)
        
        # create map/bin/hex/uf2 file etc.
        pico_add_extra_outputs(${NAME})
        target_link_libraries(${NAME} PRIVATE fmt::fmt myb::myb_headers ${MYB_EXTRA_LINKS})
        target_include_directories(${NAME} PRIVATE src)
    endfunction()
    myb_add_app(3bit_calculator src/3bit_calculator_main.cpp)
    myb_add_app(buttons_core src/buttons_core_main.cpp)
    target_link_libraries(buttons_core PRIVATE hardware_pwm hardware_adc hardware_dma fmt::fmt)
    
    # enable usb output, disable uart output
    pico_enable_stdio_usb(${TEST_NAME} 1)
    pico_enable_stdio_uart(${TEST_NAME} 0)

    # create map/bin/hex/uf2 file etc.
    pico_add_extra_outputs(${TEST_NAME})

    add_compile_definitions(${TEST_NAME} MYB_PICO=true)
endif ()

target_link_libraries(${TEST_NAME} PRIVATE fmt::fmt cta::cta myb::myb_headers ${MYB_EXTRA_LINKS})

