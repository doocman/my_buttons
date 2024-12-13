
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

struct do_init_t {};

template <typename Fetcher>
  requires(std::is_empty_v<Fetcher> && std::invocable<Fetcher> &&
           std::is_lvalue_reference_v<std::invoke_result_t<Fetcher>>)
struct static_toggle_wrapper : private Fetcher {
  constexpr decltype(auto) trigger() noexcept { return Fetcher::trigger(); }
  constexpr decltype(auto) on_sleep() noexcept { return Fetcher::on_sleep(); }
  constexpr decltype(auto) on_wake() noexcept { return Fetcher::on_wake(); }
};

template <typename Trigger> class no_sleep_wake : Trigger {
public:
  constexpr explicit no_sleep_wake(Trigger t) : Trigger(std::move(t)) {}
  constexpr no_sleep_wake() = default;
  constexpr void trigger() noexcept { static_cast<Trigger &>(*this)(); }
  static constexpr void on_sleep() noexcept {};
  static constexpr void on_wake() noexcept {};
};
template <typename T>
no_sleep_wake(T &&) -> no_sleep_wake<std::remove_cvref_t<T>>;

inline int _init_gpio_for_output(unsigned pin) {
  gpio_init(pin);
  gpio_set_dir(pin, GPIO_OUT);
  return {};
}
inline int _set_gpio_out(unsigned pin, bool value) {
  gpio_put(pin, value ? 1 : 0);
  return {};
}

template <ct_int pin> struct rxtx_wake_interrupt {
  rxtx_wake_interrupt() noexcept { _init_gpio_for_output(pin.i); }
  void set() const { gpio_put(pin.i, 1); }
  void reset() const { gpio_put(pin.i, 0); }
};

struct led_binary_out_base {
  template <ct_int... pins, std::size_t... is>
    requires(sizeof...(is) == sizeof...(pins))
  static void do_set(std::bitset<sizeof...(pins)> const &vals,
                     std::index_sequence<is...>) {
    (void)(_set_gpio_out(pins.i, vals[is]) + ...);
  }
};

template <ct_int... pins> struct led_binary_out : private led_binary_out_base {

  static void init() { (void)(_init_gpio_for_output(pins.i) + ...); }

  static void set(std::bitset<sizeof...(pins)> const &vals) {
    do_set<pins...>(vals, std::make_index_sequence<sizeof...(pins)>{});
  }
  static void sleep() { (void)(_set_gpio_out(pins.i, 0) + ...); }
  static constexpr auto out_size = sizeof...(pins);
};

template <typename T>
concept led_binary_out_c = requires() {
  { T::out_size } -> std::convertible_to<std::size_t>;
  T::init();
} && requires(std::bitset<T::out_size> const &bits) { T::set(bits); };

template <typename T, std::size_t sz>
concept led_binary_out_sized = led_binary_out_c<T> && T::out_size == sz;

template <typename T, typename U>
concept same_led_binary_out_as =
    led_binary_out_c<U> && led_binary_out_sized<T, U::out_size>;

template <typename T, typename U>
concept output_led_out_for =
    led_binary_out_c<U> && led_binary_out_sized<T, U::out_size * 2>;

template <led_binary_out_c LHS, same_led_binary_out_as<LHS> RHS,
          output_led_out_for<LHS> RES, led_binary_out_sized<2> OP>
struct calc_output {
  using in_bits = std::bitset<LHS::out_size>;
  using out_bits = std::bitset<RES::out_size>;
  using op_bits = std::bitset<2>;
  static constexpr void set_lhs(in_bits const &v) { LHS::set(v); }
  static constexpr void set_rhs(in_bits const &v) { RHS::set(v); }
  static constexpr void set_result(out_bits const &v) { RES::set(v); }
  static constexpr void set_no_result() {}
  static constexpr void set_operator(op_bits const &v) { OP::set(v); }
  static constexpr void init_all() {
    LHS::init();
    RHS::init();
    RES::init();
    OP::init();
  }
  static constexpr void sleep_all() {
    LHS::sleep();
    RHS::sleep();
    RES::sleep();
    OP::sleep();
  }
};

static auto calc_3b = few_buttons_calculator<3>();
static auto calc_wrap = calc_2_led([]() -> auto & { return calc_3b; });
static auto wake_other = rxtx_wake_interrupt<8>();

template <std::invocable Getter, typename Output> class rotate_calc3b : Getter {
  constexpr auto &get_wrap() { return static_cast<Getter &>(*this)(); }

public:
  constexpr rotate_calc3b(Getter g, std::type_identity<Output>, do_init_t)
      : Getter(std::move(g)) {
    Output::init_all();
  }
  constexpr rotate_calc3b() = default;
  constexpr void trigger() noexcept {
    auto &c = get_wrap();
    c.rotate_behaviour(Output{});
  }
  constexpr void on_sleep() noexcept { Output::sleep_all(); }
  constexpr void on_wake() noexcept { get_wrap().read_all(Output{}); }
};

// gpio 0+1 -> i2c to other RPi Pico
// gpio 2,3,4,5 -> reserved for future SPIO or i2c.
// gpio 6 -> send wake interrupt
// gpio 7 -> receive wake interrupt

using calc_op_pins = led_binary_out<8, 9>;
using calc_rhs_out_pins = led_binary_out<13, 14, 15>;
using calc_lhs_out_pins = led_binary_out<17, 18, 19>;
using calc_res_out_pins = led_binary_out<20, 21, 22, 26, 27, 28>;
using calc_output_t = calc_output<calc_lhs_out_pins, calc_rhs_out_pins,
                                  calc_res_out_pins, calc_op_pins>;

static auto ui_context_calc =
    ui_context::builder()
        .gpios( //
            gpio_sel<16> >> rotate_calc3b([]() -> auto & { return calc_wrap; },
                                          std::type_identity<calc_output_t>{},
                                          do_init_t{}), //
            gpio_sel<10> >> no_sleep_wake([] {
              calc_wrap.template toggle_bit<0>(calc_output_t{});
            }), //
            gpio_sel<11> >> no_sleep_wake([] {
              calc_wrap.template toggle_bit<1>(calc_output_t{});
            }), //
            gpio_sel<12> >> no_sleep_wake([] {
              calc_wrap.template toggle_bit<2>(calc_output_t{});
            }) //
            )
        .build();

void wake_and_prolong_no_send() {}
void wake_and_prolong() {
  wake_and_prolong_no_send();
  wake_other.set();
}

void gpio_irq(uint gpio, std::uint32_t events) {
  constexpr std::uint32_t edge_rise_mask = 0b1000u;
  // We only care about edge rise.
  if ((events & edge_rise_mask) == 0) {
    return;
  }
  if (gpio == 7) {
    wake_and_prolong_no_send();
  } else {
    ui_context_calc.trigger_gpio(gpio, [] { wake_and_prolong(); });
  }
}

int main() {
  while (1) {
  }
}
} // namespace
} // namespace myb

int main() { return myb::main(); }
