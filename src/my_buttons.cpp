
#include <chrono>
#include <thread>

#include "pico/stdlib.h"

#include <fmt/core.h>

#include <picolinux/picolinux_libc.hpp>

namespace myb {
inline namespace {
int main() {
    stdio_init_all();
    while(true) {
        if(stdio_usb_connected()) {
            fmt::print("USB Connected!\n");
            break;
        }
    }
    while(true) {
        fmt::print("Hello World!\n");
        //sleep_us(1000);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}
}
}

int main() {
    return myb::main();
}
