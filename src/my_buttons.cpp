
#include <chrono>
#include <functional>
#include <thread>
#include <type_traits>

#include "pico/stdlib.h"

#include <fmt/core.h>

#include <picolinux/picolinux_libc.hpp>

#include <myb/myb.hpp>

namespace myb {
inline namespace {

template <typename Fetcher>
  requires(std::is_empty_v<Fetcher> && std::invocable<Fetcher> &&
           std::is_lvalue_reference_v<std::invoke_result_t<Fetcher>>)
struct static_toggle_wrapper : private Fetcher {
  constexpr decltype(auto) trigger() noexcept { return Fetcher::trigger(); }
  constexpr decltype(auto) on_sleep() noexcept { return Fetcher::on_sleep(); }
  constexpr decltype(auto) on_wake() noexcept { return Fetcher::on_wake(); }
};

template <ct_int pin> struct rxtx_wake_interrupt {
  rxtx_wake_interrupt() noexcept {
    gpio_init(pin.i);
    gpio_set_dir(pin.i, GPIO_OUT);
  }
  void operator()() const { gpio_put(pin.i, 1); }
  void reset() const { gpio_put(pin.i, 0); }
};

static auto calc_3b = few_buttons_calculator<3>();
static auto wake_other = rxtx_wake_interrupt<8>();

struct rotate_calc3b {
  constexpr void trigger() noexcept { rotate_inplace(calc_3b); }
  constexpr void on_sleep() noexcept {}
  constexpr void on_wake() noexcept {}
};

// gpio 0+1 -> i2c to other RPi Pico
// gpio 2,3,4,5,6,7 -> reserved for future SPIO or i2c.
// gpio 8 -> send wake interrupt
// gpio 9 -> receive wake interrupt

static auto ui_context_calc = ui_context::builder()
                                  .gpios( //
                                          //
                                      )
                                  .build();

void wake_and_prolong();

void gpio_irq(uint gpio, std::uint32_t events) {
  constexpr std::uint32_t edge_rise_mask = 0b1000u;
  // We only care about edge rise.
  if ((events & edge_rise_mask) == 0) {
    return;
  }
  if (gpio == 9 || ui_context_calc.trigger_gpio(gpio)) {
    wake_and_prolong();
  }
}

int main() {
  while (1) {
  }
}
} // namespace
} // namespace myb

int main() { return myb::main(); }
