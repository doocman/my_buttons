
#ifndef MY_BUTTONS_MYB_MYB_HPP
#define MY_BUTTONS_MYB_MYB_HPP

#include <utility>

#include <cgui/std-backport/tuple.hpp>

namespace myb {
namespace dtl = cgui::bp;
template <typename T>
concept gpio_action = requires(T& t) {
    t.trigger();
};
template <typename Int>
struct ct_int {
    Int i;
    constexpr explicit(false) ct_int(Int in) noexcept : i(in) {}
    constexpr bool operator==(ct_int const&) const = default;
    constexpr auto operator<=>(ct_int const&) const = default;
};
template <typename Int>
ct_int(Int) -> ct_int<Int>;
template <ct_int pin, gpio_action Action>
class gpio_action_t : dtl::empty_structs_optimiser<Action> {
    using _base_t = dtl::empty_structs_optimiser<Action>;
public:
static constexpr auto pin_value = pin.i;
constexpr decltype(auto) trigger() {
    return this->get_first().trigger();
}
template <typename... Ts>
requires(std::constructible_from<_base_t, Ts...>)
constexpr explicit(sizeof...(Ts) == 1) gpio_action_t(Ts&&... args) : _base_t(std::forward<Ts>(args)...) {}
};
class ui_context {
    template <typename GPIOs>
    class impl : GPIOs {
    public:
        template <typename GP>
        requires(std::constructible_from<GPIOs, GP>)
        constexpr explicit impl(GP&& g) : GPIOs(std::forward<GP>(g)) {}
        constexpr bool toggle_gpio(std::integral auto pin) {
            // TODO: Change here to use some smarter fetching of correct pin. Or at least check godbolt for it's compilation.
            return apply_to(static_cast<GPIOs&>(*this), [pin] (auto&... actions) -> bool {
                auto constexpr invoker = [] (auto& a, auto p) {
                    if (a.pin_value == p) {
                        a.trigger();
                        return true;
                    }
                    return false;
                };
                return (invoker(actions, pin) || ...);
            });
        }
    };
public:
    template <typename GPIOs>
    class builder_t {
        GPIOs gpios_;
    public:
        constexpr builder_t() = default;
        template <typename T>
        requires(std::constructible_from<GPIOs, T>)
        constexpr explicit builder_t(T&& gpios) : gpios_(std::forward<T>(gpios)) {}
        template <ct_int... pins, typename... Actions, typename TupleType = dtl::empty_structs_optimiser<gpio_action_t<pins, Actions>...>>
        constexpr builder_t<TupleType> gpios(gpio_action_t<pins, Actions>&&... actions) && { 
            return builder_t<TupleType>(TupleType(std::move(actions)...)); }
        constexpr impl<GPIOs> build() && { return impl<GPIOs>(std::move(*this).gpios_); }
    };
    static constexpr builder_t<std::tuple<>> builder() { return {}; }

    
    
};

template <ct_int>
class gpio_sel_t {};
template <ct_int pin>
static constexpr gpio_sel_t<pin> gpio_sel{};
template <ct_int pin, gpio_action Action, typename ResType = gpio_action_t<pin, std::remove_cvref_t<Action>>>
constexpr ResType operator>>(gpio_sel_t<pin>, Action&& action) { return ResType(std::forward<Action>(action)); }
template <ct_int pin, gpio_action Action, typename ResType = gpio_action_t<pin, Action&>>
constexpr ResType operator>>(gpio_sel_t<pin>, std::reference_wrapper<Action> action) { return ResType(action.get()); }

}

#endif
