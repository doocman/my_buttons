
#ifndef MYB_APP_MYB_APP_HPP
#define MYB_APP_MYB_APP_HPP

#include <chrono>
#include <concepts>

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

template <typename T>
  requires(requires() { T::reset(); })
struct call_static_reset {
  constexpr auto operator()(auto &&...) const -> decltype(T::reset()) {
    return T::reset();
  }
};

struct queue_reset {
  static constexpr auto timeout = std::chrono::microseconds(100);
  template <typename T, typename Queue>
    requires(requires() { T::reset(); })
  constexpr auto operator()(T const &,
                            Queue &&q) const -> decltype(T::reset()) {
    // return T::reset();
    q.que(call_static_reset<T>{}, std::chrono::steady_clock::now() + timeout);
  }
};

template <ct_int pin, typename QueueReset = queue_reset>
  requires(std::is_empty_v<QueueReset>)
struct rxtx_wake_interrupt {
  constexpr rxtx_wake_interrupt() = default;
  constexpr explicit rxtx_wake_interrupt(QueueReset) {}
  static void set(auto &&q) {
    gpio_put(pin.i, 1);
    QueueReset{}(rxtx_wake_interrupt{}, q);
  }
  static void reset() { gpio_put(pin.i, 0); }
  static void init() { init_gpio_for_output(pin.i); }
};

void go_deep_sleep() { scb_hw->scr |= ARM_CPU_PREFIXED(SCR_SLEEPDEEP_BITS); }
} // namespace
} // namespace myb

#endif
