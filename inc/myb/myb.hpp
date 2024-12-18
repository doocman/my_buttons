
#ifndef MY_BUTTONS_MYB_MYB_HPP
#define MY_BUTTONS_MYB_MYB_HPP

#include <bitset>
#include <climits>
#include <concepts>
#include <cstdint>
#include <limits>
#include <numeric>
#include <type_traits>
#include <utility>

#include <cgui/std-backport/functional.hpp>
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
struct no_op_gpio_action_t {
  static constexpr void trigger(auto &&...) noexcept {}
  static constexpr void on_sleep(auto &&...) noexcept {}
  static constexpr void on_wake(auto &&...) noexcept {}
};
inline constexpr no_op_gpio_action_t no_op_gpio_action;

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
    template <std::integral Pin, std::invocable CB = dtl::no_op_t>
    constexpr bool trigger_gpio(Pin pin, CB &&cb = {}) {
      if (apply_to(static_cast<GPIOs &>(*this),
                   [this, pin](auto &&...actions) -> bool {
                     auto constexpr invoker = [](auto &a, auto p, auto &&self) {
                       if (a.pin_value == p) {
                         a.trigger();
                         return true;
                       }
                       return false;
                     };
                     return (invoker(actions, pin, *this) || ...);
                   })) {
        std::invoke(cb);
        return true;
      }
      return false;
    }
    constexpr void sleep() {
      if (!sleeping) {
        apply_to(static_cast<GPIOs &>(*this), [](auto &&...actions) {
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
        apply_to(static_cast<GPIOs &>(*this), [](auto &&...actions) {
          auto constexpr waker = [](auto &a) {
            a.on_wake();
            return 0;
          };
          (void)(waker(actions) + ...);
        });
        sleeping = false;
      }
    }
    constexpr void for_each_input(std::invocable<uint> auto &&cb) const {
      apply_to(static_cast<GPIOs const &>(*this),
               [&cb](auto const &...actions) {
                 constexpr auto inv = [](auto &c, auto &a) {
                   std::invoke(c, a.pin_value);
                   return 0;
                 };
                 (void)(inv(cb, actions) + ...);
               });
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

template <typename T, typename... Ts>
constexpr variant_stateless_function<T, Ts...>
rotate(variant_stateless_function<T, Ts...> vsf) {
  constexpr auto max_v = sizeof...(Ts);
  vsf.index((vsf.index() + 1) % max_v);
  return vsf;
}
template <typename T, typename... Ts>
constexpr void rotate_inplace(variant_stateless_function<T, Ts...> *vsf) {
  *vsf = rotate(*vsf);
}

enum class few_buttons_calculator_operations {
  add,
  subtract,
  multiply,
  divide
};

template <std::size_t bit_count> class few_buttons_calculator {
public:
  static constexpr auto input_bits = bit_count;
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
  struct multiply : op_base<multiply> {
    static constexpr result_t call(input_t l, input_t r) { return l * r; }
  };
  struct divide : op_base<divide> {
    static constexpr result_t call(input_t l, input_t r) {
      if (r != 0) {
        auto qr = std::div(l, r);
        return (qr.quot << 3) | qr.rem;
      } else {
        return {};
      }
    }
  };

  input_t lhs_{};
  input_t rhs_{};
  variant_stateless_function<std::pair<input_t, input_t>, plus, minus, multiply,
                             divide>
      op_;

public:
  constexpr result_t result() const { return op_(std::pair{lhs_, rhs_}); }
  constexpr void set_lhs(input_t v) { lhs_ = v; }
  constexpr void set_rhs(input_t v) { rhs_ = v; }
  constexpr input_t lhs() const noexcept { return lhs_; }
  constexpr input_t rhs() const noexcept { return rhs_; }
  constexpr void set_operator(few_buttons_calculator_operations op) {
    op_.index(static_cast<int>(op));
  }
  constexpr few_buttons_calculator_operations
  current_operator() const noexcept {
    return static_cast<few_buttons_calculator_operations>(op_.index());
  }
  constexpr void swap_lr() noexcept { std::swap(lhs_, rhs_); }
  constexpr bool can_compute() const {
    return op_.index() !=
               static_cast<int>(few_buttons_calculator_operations::divide) ||
           rhs_ != 0;
  }

  friend constexpr void rotate_inplace(few_buttons_calculator *calc) {
    rotate_inplace(&calc->op_);
  }
};

template <typename T>
concept few_buttons_calculator_like = requires(T &t, T const &tc, int i) {
  { std::remove_cvref_t<T>::input_bits } -> std::convertible_to<std::size_t>;
  t.set_lhs(i);
  { t.lhs() } -> std::convertible_to<unsigned>;
  t.swap_lr();
};

struct _calc_2_led_base {
  enum class state { val0, val1, op };
  state s_ = state::val0;
};

template <typename T, std::size_t in_bits>
concept calc2led_callback =
    requires(T &&t, std::bitset<in_bits> lr, std::bitset<in_bits * 2> res,
             std::bitset<2> op) {
      t.set_lhs(lr);
      t.set_rhs(lr);
      t.set_result(res);
      t.set_no_result();
      t.set_operator(op);
    };

template <std::invocable T>
  requires(few_buttons_calculator_like<std::invoke_result_t<T>> &&
           std::is_reference_v<std::invoke_result_t<T>>)
class calc_2_led : _calc_2_led_base {
  [[no_unique_address]] T getter_{};
  using calc_t = std::remove_cvref_t<std::invoke_result_t<T>>;
  static constexpr auto input_bits = calc_t::input_bits;

  constexpr calc_t &get() { return getter_(); }

  constexpr auto update_result(auto &cb) {
    auto &c = get();
    if (c.can_compute()) {
      cb.set_result(
          std::bitset<input_bits * 2>(static_cast<unsigned long>(c.result())));
    } else {
      cb.set_no_result();
    }
  }
  constexpr void update_op(auto &&cb) {
    auto &c = get();
    auto op = c.current_operator();
    cb.set_operator(std::bitset<2>(static_cast<unsigned long>(op)));
  }

  constexpr auto to_in_set(auto val) {
    return std::bitset<input_bits>(static_cast<unsigned long>(val));
  }

public:
  constexpr calc_2_led() = default;
  constexpr explicit calc_2_led(auto &&...gs)
      : getter_(std::forward<decltype(gs)>(gs)...) {}

  template <std::size_t bit, calc2led_callback<input_bits> CB>
    requires(bit < input_bits || bit < 2)
  constexpr void toggle_bit(CB &&cb) {
    constexpr unsigned xor_mask = (1u << bit);
    auto &c = get();
    if (s_ != state::op) {
      auto org_val = c.rhs();
      org_val ^= xor_mask;
      c.set_rhs(org_val);
      cb.set_rhs(to_in_set(c.rhs()));
      update_result(cb);
    } else {
      if constexpr (bit < 2) {
        auto op = c.current_operator();
        auto new_op = static_cast<unsigned>(op) ^ xor_mask;
        c.set_operator(static_cast<few_buttons_calculator_operations>(new_op));
        update_result(cb);
        update_op(cb);
      }
    }
  }
  template <calc2led_callback<input_bits> CB> constexpr void read_all(CB &&cb) {
    auto &c = get();
    cb.set_lhs(to_in_set(c.lhs()));
    cb.set_rhs(to_in_set(c.rhs()));
    update_result(cb);
    update_op(cb);
  }
  template <calc2led_callback<input_bits> CB>
  constexpr void rotate_behaviour(CB &&cb) {
    if (s_ == state::val0) {
      auto &c = get();
      c.swap_lr();
      read_all(cb);
    }
    s_ = static_cast<state>((static_cast<int>(s_) + 1) % 3);
  }
};
template <typename T>
calc_2_led(T &&) -> calc_2_led<std::unwrap_ref_decay_t<T>>;

template <typename TimePoint, typename... Ts>
class typed_time_queue : dtl::empty_structs_optimiser<Ts...> {
  using _base_t = dtl::empty_structs_optimiser<Ts...>;
  static constexpr auto q_size = sizeof...(Ts);
  static consteval std::array<TimePoint, q_size> init_tps() {
    std::array<TimePoint, q_size> res{};
    std::ranges::fill(res, TimePoint::max());
    return res;
  }
  std::array<TimePoint, q_size> time_points_ = init_tps();
  using ts_type_erase =
      std::add_pointer_t<void(typed_time_queue &, TimePoint const &)>;
  template <typename T>
  inline static constexpr auto type_index_v =
      dtl::tuple_element_index_v<T, std::tuple<Ts...>>;
  inline static constexpr std::array<ts_type_erase, q_size> ts_callbacks = {
      [](typed_time_queue &q, TimePoint const &tp) {
        get<type_index_v<Ts>>(static_cast<_base_t &>(q))(q, tp);
      }...};
  constexpr auto next_element() {
    return std::ranges::min_element(time_points_);
  }

public:
  constexpr typed_time_queue() = default;
  template <typename... Us>
    requires(std::constructible_from<_base_t, Us...>)
  constexpr explicit typed_time_queue(TimePoint, Us &&...args)
      : _base_t(std::forward<Us>(args)...) {}
  constexpr std::optional<TimePoint> next() {
    if (auto val = next_element(); *val < TimePoint::max()) {
      return *val;
    }
    return std::nullopt;
  }
  constexpr int execute_all(TimePoint const &tp) {
    int count{};
    while (true) {
      auto lowest = std::ranges::min_element(time_points_);
      auto cur_tp = *lowest;
      if (cur_tp <= tp) {
        ++count;
        *lowest = TimePoint::max();
        auto index = std::ranges::distance(begin(time_points_), lowest);
        ts_callbacks[index](*this, cur_tp);
      } else {
        return count;
      }
    }
  }
  template <typename T>
    requires((std::is_same_v<T, Ts> || ...))
  constexpr void que(T const &, TimePoint tp) {
    constexpr auto type_i = dtl::tuple_element_index_v<T, std::tuple<Ts...>>;
    static_assert(type_i < q_size);
    time_points_[type_i] = tp;
  }
  template <typename T>
    requires((std::is_same_v<T, Ts> || ...))
  constexpr void unque(T const &) {
    constexpr auto type_i = dtl::tuple_element_index_v<T, std::tuple<Ts...>>;
    static_assert(type_i < q_size);
    time_points_[type_i] = TimePoint::max();
  }
};
template <typename TP, typename... Ts>
typed_time_queue(TP,
                 Ts...) -> typed_time_queue<TP, std::unwrap_ref_decay_t<Ts>...>;

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

} // namespace myb

#endif
