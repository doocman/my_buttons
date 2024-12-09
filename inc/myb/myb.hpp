
#ifndef MY_BUTTONS_MYB_MYB_HPP
#define MY_BUTTONS_MYB_MYB_HPP

#include <utility>

#include <cgui/std-backport/tuple.hpp>

namespace myb {
namespace dtl = cgui::bp;
template <typename T>
concept gpio_action = requires(T &t) {
  t.trigger();
  t.on_sleep();
  t.on_wake();
};
template <typename Int> struct ct_int {
  Int i;
  constexpr explicit(false) ct_int(Int in) noexcept : i(in) {}
  constexpr bool operator==(ct_int const &) const = default;
  constexpr auto operator<=>(ct_int const &) const = default;
};
template <typename Int> ct_int(Int) -> ct_int<Int>;
template <ct_int pin, gpio_action Action>
class gpio_action_t : dtl::empty_structs_optimiser<Action> {
  using _base_t = dtl::empty_structs_optimiser<Action>;

public:
  static constexpr auto pin_value = pin.i;
  constexpr decltype(auto) trigger() { return this->get_first().trigger(); }
  constexpr decltype(auto) on_sleep() { return this->get_first().on_sleep(); }
  constexpr decltype(auto) on_wake() { return this->get_first().on_wake(); }
  template <typename... Ts>
    requires(std::constructible_from<_base_t, Ts...>)
  constexpr explicit(sizeof...(Ts) == 1) gpio_action_t(Ts &&...args)
      : _base_t(std::forward<Ts>(args)...) {}
};
class ui_context {
  template <typename GPIOs> class impl : GPIOs {
    bool sleeping{};

  public:
    template <typename GP>
      requires(std::constructible_from<GPIOs, GP>)
    constexpr explicit impl(GP &&g) : GPIOs(std::forward<GP>(g)) {}
    constexpr bool toggle_gpio(std::integral auto pin) {
      return apply_to(
          static_cast<GPIOs &>(*this), [this, pin](auto &...actions) -> bool {
            auto constexpr invoker = [](auto &a, auto p, auto &&self) {
              if (a.pin_value == p) {
                self.wake();
                a.trigger();
                return true;
              }
              return false;
            };
            return (invoker(actions, pin, *this) || ...);
          });
    }
    constexpr void sleep() {
      if (!sleeping) {
        apply_to(static_cast<GPIOs &>(*this), [](auto &...actions) {
          auto constexpr sleeper = [](auto &a) {
            a.on_sleep();
            return 0;
          };
          (void)(sleeper(actions) + ...);
        });
        sleeping = true;
      }
    }
    constexpr void wake() {
      if (sleeping) {
        apply_to(static_cast<GPIOs &>(*this), [](auto &...actions) {
          auto constexpr waker = [](auto &a) {
            a.on_wake();
            return 0;
          };
          (void)(waker(actions) + ...);
        });
        sleeping = false;
      }
    }
  };

public:
  template <typename GPIOs> class builder_t {
    GPIOs gpios_;

  public:
    constexpr builder_t() = default;
    template <typename T>
      requires(std::constructible_from<GPIOs, T>)
    constexpr explicit builder_t(T &&gpios) : gpios_(std::forward<T>(gpios)) {}
    template <ct_int... pins, typename... Actions,
              typename TupleType =
                  dtl::empty_structs_optimiser<gpio_action_t<pins, Actions>...>>
    constexpr builder_t<TupleType>
    gpios(gpio_action_t<pins, Actions> &&...actions) && {
      return builder_t<TupleType>(TupleType(std::move(actions)...));
    }
    constexpr impl<GPIOs> build() && {
      return impl<GPIOs>(std::move(*this).gpios_);
    }
  };
  static constexpr builder_t<std::tuple<>> builder() { return {}; }
};

template <ct_int> class gpio_sel_t {};
template <ct_int pin> static constexpr gpio_sel_t<pin> gpio_sel{};
template <ct_int pin, gpio_action Action,
          typename ResType = gpio_action_t<pin, std::remove_cvref_t<Action>>>
constexpr ResType operator>>(gpio_sel_t<pin>, Action &&action) {
  return ResType(std::forward<Action>(action));
}
template <ct_int pin, gpio_action Action,
          typename ResType = gpio_action_t<pin, Action &>>
constexpr ResType operator>>(gpio_sel_t<pin>,
                             std::reference_wrapper<Action> action) {
  return ResType(action.get());
}

template <typename T>
concept binary_output = requires(T &t) {
  t.initiate();
  t.disable();
  t.set_on();
  t.set_off();
};
template <binary_output T>
class led_wrap_pin : dtl::empty_structs_optimiser<T> {
  using _base_t = dtl::empty_structs_optimiser<T>;
  bool is_on_{};
  bool initiated_{};
  static constexpr bool do_initiate(auto &&output) {
    output.initiate();
    return true;
  }
  constexpr void maybe_uninit() noexcept {
    if (initiated_) {
      _base_t::get_first().disable();
      initiated_ = false;
    }
  }

public:
  template <typename... Ts>
    requires(std::constructible_from<T, Ts...>)
  constexpr explicit(sizeof...(Ts) == 1) led_wrap_pin(Ts &&...args)
      : _base_t(std::forward<Ts>(args)...),
        initiated_(do_initiate(_base_t::get_first())) {}
  constexpr led_wrap_pin(led_wrap_pin &&other) noexcept
      : _base_t(std::move(other)), is_on_(other.is_on_),
        initiated_(std::exchange(other.initiated_, false)) {}
  constexpr led_wrap_pin &operator=(led_wrap_pin &&other) noexcept {
    if (!this == &other) {
      std::swap(static_cast<_base_t &>(other), static_cast<_base_t &>(*this));
      std::swap(is_on_, other.is_on_);
      std::swap(initiated_, other.initiated_);
    }
    return *this;
  }
  constexpr void turn_off() {
    if (is_on_) {
      _base_t::get_first().set_off();
      is_on_ = false;
    }
  }
  constexpr void turn_on() {
    if (is_on_) {
      _base_t::get_first().set_off();
      is_on_ = true;
    }
  }
  constexpr ~led_wrap_pin() { maybe_uninit(); }
  constexpr void trigger() {
    if (!is_on_) {
      _base_t::get_first().set_on();
    } else {
      _base_t::get_first().set_off();
    }
    is_on_ = !is_on_;
  }
};

template <typename T>
led_wrap_pin(T &&) -> led_wrap_pin<std::unwrap_ref_decay_t<T>>;

} // namespace myb

#endif
