
#include <chrono>
#include <functional>
#include <thread>
#include <type_traits>

#include <pico/stdlib.h>

#include <picolinux/picolinux_libc.hpp>

#include <app/myb_app.hpp>
#include <myb/myb.hpp>

namespace myb {
inline namespace {

struct do_init_t {};

struct led_binary_out_base {
  template <ct_int... pins, std::size_t... is>
    requires(sizeof...(is) == sizeof...(pins))
  static void do_set(std::bitset<sizeof...(pins)> const &vals,
                     std::index_sequence<is...>) {
    (void)(set_gpio_out(pins.i, vals[is]) + ...);
  }
};

template <ct_int... pins> struct led_binary_out : private led_binary_out_base {

  static void init() { (void)(init_gpio_for_output(pins.i) + ...); }

  static void set(std::bitset<sizeof...(pins)> const &vals) {
    do_set<pins...>(vals, std::make_index_sequence<sizeof...(pins)>{});
  }
  static void sleep() { (void)(set_gpio_out(pins.i, 0) + ...); }
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
          output_led_out_for<LHS> RES, led_binary_out_sized<2> OP,
          std::invocable NO_RES>
  requires(std::is_empty_v<NO_RES>)
struct calc_output {
  using in_bits = std::bitset<LHS::out_size>;
  using out_bits = std::bitset<RES::out_size>;
  using op_bits = std::bitset<2>;
  static constexpr void set_lhs(in_bits const &v) { LHS::set(v); }
  static constexpr void set_rhs(in_bits const &v) { RHS::set(v); }
  static constexpr void set_result(out_bits const &v) { RES::set(v); }
  static constexpr void set_lresult(unsigned long long v) {
    set_result(out_bits(v));
  }
  static constexpr void set_no_result() { std::invoke(NO_RES{}); }
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
  constexpr void on_wake() noexcept {
    Output::init_all();
    get_wrap().read_all(Output{});
  }
};

// gpio 0+1 -> i2c to other RPi Pico
// gpio 2,3,4,5 -> reserved for future SPIO or i2c.
// gpio 6 -> send wake interrupt
// gpio 7 -> receive wake interrupt

inline constexpr uint wake_tx_gpio = 6u;
inline constexpr uint wake_rx_gpio = 7u;

using steady_clock = std::chrono::steady_clock;
inline constexpr auto sleep_timeout = std::chrono::minutes(5);
inline constexpr auto sleep_poll_timeout = std::chrono::seconds(5);
static auto next_sleep = steady_clock::time_point{};
static auto calc_3b = few_buttons_calculator<3>();
static auto calc_wrap = calc_2_led([]() -> auto & { return calc_3b; });
static auto wake_other = rxtx_wake_interrupt<wake_tx_gpio>();

inline constexpr auto calc_no_res_flash_timeout = std::chrono::seconds(1);
static std::optional<steady_clock::time_point> next_calc_flash;

using calc_op_pins = led_binary_out<8, 9>;
using calc_rhs_out_pins = led_binary_out<13, 14, 15>;
using calc_lhs_out_pins = led_binary_out<17, 18, 19>;
using calc_res_out_pins = led_binary_out<20, 21, 22, 26, 27, 28>;
using calc_output_t =
    calc_output<calc_lhs_out_pins, calc_rhs_out_pins, calc_res_out_pins,
                calc_op_pins, decltype([]() {
                  using namespace std::chrono;
                  next_calc_flash = steady_clock::now();
                })>;

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

void wake_and_prolong_no_send(steady_clock::time_point now) {
  wake_other.init();
  next_sleep = now + sleep_timeout;
}
void wake_and_prolong_no_send() {
  wake_and_prolong_no_send(steady_clock::now());
}
void wake_and_prolong() {
  wake_and_prolong_no_send();
  wake_other.set();
}
void sleep() {
  ui_context_calc.sleep();
  next_calc_flash = std::nullopt;
  calc_output_t::sleep_all();
  go_deep_sleep();
}

std::optional<std::chrono::microseconds> run_async_tasks(auto now) {
  using namespace std::chrono;
  if (next_calc_flash) {
    if (calc_3b.can_compute()) {
      next_calc_flash = std::nullopt;
    } else {
      auto &next_flash = *next_calc_flash;
      while (now >= next_flash) {
        auto is_flash_on =
            (duration_cast<seconds>(next_flash.time_since_epoch()).count() &
             1) == 1;
        auto val_to_set = is_flash_on
                              ? std::numeric_limits<unsigned long>::max()
                              : (unsigned long){};
        calc_output_t::set_lresult(val_to_set);
        next_flash += calc_no_res_flash_timeout;
      }
      return duration_cast<microseconds>(next_flash - now);
    }
  }
  return std::nullopt;
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

void main() {
  while (1) {

    gpio_set_dir(wake_rx_gpio, GPIO_IN);
    gpio_set_irq_enabled_with_callback(wake_rx_gpio, GPIO_IRQ_EDGE_RISE, true,
                                       &gpio_irq);
    gpio_set_dormant_irq_enabled(wake_rx_gpio, GPIO_IRQ_EDGE_RISE, true);
    ui_context_calc.for_each_input([](uint pin) {
      gpio_set_dir(pin, GPIO_IN);
      gpio_set_irq_enabled(pin, GPIO_IRQ_EDGE_RISE, true);
      gpio_set_dormant_irq_enabled(pin, GPIO_IRQ_EDGE_RISE, true);
    });
    auto now_time = steady_clock::now();
    wake_and_prolong_no_send(now_time);
    while (now_time < next_sleep) {
      auto next_sleep = run_async_tasks(now_time);
      std::this_thread::sleep_for(next_sleep.value_or(sleep_poll_timeout));
      now_time = steady_clock::now();
    }
    sleep();
  }
}
} // namespace
} // namespace myb

int main() { myb::main(); }
