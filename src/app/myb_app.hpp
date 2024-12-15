
#ifndef MYB_APP_MYB_APP_HPP
#define MYB_APP_MYB_APP_HPP

#include <pico/stdlib.h>

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
} // namespace
} // namespace myb

#endif
