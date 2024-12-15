
#include "pico/stdlib.h"

#include <picolinux/picolinux_libc.hpp>

#include <myb/myb.hpp>

namespace myb {

// gpio 0+1 -> i2c to other RPi Pico
// gpio 2,3,4,5 -> reserved for future SPIO or i2c.
// gpio 6 -> send wake interrupt
// gpio 7 -> receive wake interrupt

void main() {}
} // namespace myb

int main() {
  while (1) {
    myb::main();
  }
}
