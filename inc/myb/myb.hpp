
#ifndef MY_BUTTONS_MYB_MYB_HPP
#define MY_BUTTONS_MYB_MYB_HPP

#include <numeric>
#include <utility>

#include <cgui/std-backport/tuple.hpp>
#include <cgui/std-backport/utility.hpp>

namespace myb {
constexpr void always_assert(auto &&...conditions) {
  if (!(conditions && ...)) [[unlikely]] {
    std::abort();
  }
}
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

public:
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

private:
  constexpr void maybe_uninit() noexcept {
    if (initiated_) {
      turn_off();
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

template <typename T, typename Data>
concept variant_behaviour_member =
    std::invocable<T, Data &> && std::is_empty_v<T> && std::is_trivial_v<T>;

template <typename Data, variant_behaviour_member<Data>... Behaviours>
  requires(sizeof...(Behaviours) > 0)
class variant_stateless_function {
  using return_t =
      std::common_reference_t<std::invoke_result_t<Behaviours, Data>...>;
  using f_type = return_t(Data &&);
  template <std::size_t i>
  using behaviour_t = std::tuple_element_t<i, std::tuple<Behaviours...>>;
  inline static constexpr std::add_pointer_t<f_type> functions[] = {
      [](Data &&d) -> return_t {
        return Behaviours{}(std::forward<Data>(d));
      }...};
  using index_t = std::uint_least8_t;
  static_assert(std::numeric_limits<index_t>::max() >=
                sizeof...(Behaviours) - 1);
  index_t i_{};

public:
  template <typename D2>
    requires(std::constructible_from<Data, D2>)
  explicit constexpr variant_stateless_function(D2 &&, Behaviours...) {}
  explicit constexpr variant_stateless_function(Behaviours...) {}
  constexpr variant_stateless_function() = default;

  static constexpr std::size_t size() noexcept { return sizeof...(Behaviours); }
  constexpr return_t operator()(Data &&d) const {
    return functions[i_](std::forward<Data>(d));
  }
  constexpr std::size_t index() const noexcept {
    return static_cast<std::size_t>(i_);
  }
  constexpr void index(std::size_t i) {
    always_assert(i < size());
    i_ = static_cast<index_t>(i);
  }
};

template <typename T, typename... Ts>
variant_stateless_function(T &&, Ts...)
    -> variant_stateless_function<std::unwrap_ref_decay_t<T>, Ts...>;

enum class few_buttons_calculator_operations {
  add,
  subtract,
  multiply,
  divide
};

template <std::size_t bit_count> class few_buttons_calculator {
public:
  using result_t = std::uint_least8_t;
  static_assert(sizeof(result_t) * CHAR_BIT >= bit_count * 2);
  using input_t = result_t;
  static constexpr auto max_in = ((1 << bit_count) - 1);
  static_assert(((max_in + 1) >> bit_count) == 1,
                "We can't represent bit_count");
  static_assert(max_in <= std::numeric_limits<input_t>::max());

private:
  static constexpr result_t result_mask = (1 << (bit_count * 2)) - 1;
  static_assert(result_mask > max_in);
  static_assert(((result_mask + 1) >> (bit_count * 2)) == 1,
                "We can't represent bit_count");
  template <typename B> struct op_base {
    constexpr result_t operator()(std::pair<input_t, input_t> v) const {
      return B::call(v.first, v.second);
    }
  };
  struct plus : op_base<plus> {
    static constexpr result_t call(input_t l, input_t r) { return l + r; }
  };
  struct minus : op_base<minus> {
    static constexpr result_t call(input_t l, input_t r) {
      return static_cast<result_t>(l - r) & result_mask;
    }
  };
  struct subtract : op_base<subtract> {
    static constexpr result_t call(input_t l, input_t r) { return l + r; }
  };
  struct divide : op_base<divide> {
    static constexpr result_t call(input_t l, input_t r) { return l + r; }
  };

  input_t lhs_{};
  input_t rhs_{};
  variant_stateless_function<std::pair<input_t, input_t>, plus, minus, subtract,
                             divide>
      op_;

public:
  constexpr result_t result() const {
    // return lhs_ + rhs_;
    return op_(std::pair{lhs_, rhs_});
  }
  constexpr void set_lhs(input_t v) { lhs_ = v; }
  constexpr void set_rhs(input_t v) { rhs_ = v; }
  constexpr void set_operator(few_buttons_calculator_operations op) {
    op_.index(static_cast<int>(op));
  }
  constexpr void swap_lr() noexcept {
    // std::swap(lhs_, rhs_);
  }
};

} // namespace myb

#endif
