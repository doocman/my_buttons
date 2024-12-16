
#ifndef MYB_APP_MYB_APP_HPP
#define MYB_APP_MYB_APP_HPP

#include <pico/stdlib.h>

#include <myb/myb.hpp>

namespace myb {
inline namespace {
int init_gpio_for_output(unsigned pin) {
  gpio_init(pin);
  gpio_set_dir(pin, GPIO_OUT);
  return {};
}
int set_gpio_out(unsigned pin, bool value) {
  gpio_put(pin, value ? 1 : 0);
  return {};
}

template <ct_int pin> struct rxtx_wake_interrupt {
  void set() const { gpio_put(pin.i, 1); }
  void reset() const { gpio_put(pin.i, 0); }
  void init() const { init_gpio_for_output(pin.i); }
};
} // namespace
} // namespace myb

#endif
