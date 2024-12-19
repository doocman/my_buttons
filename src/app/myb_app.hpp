
#ifndef MYB_APP_MYB_APP_HPP
#define MYB_APP_MYB_APP_HPP

#include <chrono>
#include <concepts>

#include <hardware/sync.h>
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

class alarm_t {
  using steady_clock = std::chrono::steady_clock;
  using time_point = steady_clock::time_point;
  time_point alarm_time = time_point::min();
  alarm_id_t alarm_id{};
  constexpr absolute_time_t tp2picotime(time_point const &tp) noexcept {
    using namespace std::chrono;
    auto raw_value = static_cast<std::uint64_t>(
        duration_cast<microseconds>(tp.time_since_epoch()).count());
    return {raw_value};
  }
  void _do_cancel() {
    if (has_alarm()) {
      cancel_alarm(alarm_id);
    }
  }

public:
  static constexpr auto default_callback = [](auto...) -> std::int64_t {
    return 0;
  };
  constexpr alarm_t() noexcept = default;
  alarm_t(time_point tp, alarm_callback_t cb) noexcept
      : alarm_time(tp),
        alarm_id(add_alarm_at(tp2picotime(tp), cb, nullptr, false)) {}
  explicit alarm_t(time_point tp) noexcept : alarm_t(tp, default_callback) {}
  constexpr bool has_alarm() const { return alarm_time != time_point::min(); }
  void cancel() {
    _do_cancel();
    alarm_time = time_point::min();
  }
  ~alarm_t() { _do_cancel(); }
  alarm_t(alarm_t &&other) noexcept
      : alarm_time(std::exchange(other.alarm_time, time_point::min())),
        alarm_id(other.alarm_id) {}
  alarm_t &operator=(alarm_t &&other) noexcept {
    if (this != &other) {
      _do_cancel();
      other.alarm_time = time_point::min();
      alarm_id = other.alarm_id;
    }
    return *this;
  }
  constexpr time_point alarm_point() const { return alarm_time; }
};

void go_deep_sleep() { scb_hw->scr |= ARM_CPU_PREFIXED(SCR_SLEEPDEEP_BITS); }
} // namespace
} // namespace myb

#endif
