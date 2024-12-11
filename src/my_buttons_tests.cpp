
#include <array>
#include <chrono>
#include <thread>

#ifdef MYB_PICO
#include <class/cdc/cdc_device.h>
#include <device/usbd.h>
#include <pico/bootrom.h>
#include <pico/stdlib.h>
#include <picolinux/picolinux_libc.hpp>
#endif

#include <fmt/core.h>

#include <myb/myb.hpp>

#include <cta/cta.hpp>

namespace myb {
using namespace ::cta;
struct dummy_toggle {
  int toggle_count{};
  int sleep_count{};
  int wake_count{};

  constexpr void trigger() noexcept { ++toggle_count; }
  constexpr void on_sleep() noexcept { ++sleep_count; }
  constexpr void on_wake() noexcept { ++wake_count; }
};
struct dummy_output_pin {
  int initiated{};
  int disabled{};
  int toggled_on{};
  int toggled_off{};
  constexpr void set_on() { ++toggled_on; }
  constexpr void set_off() { ++toggled_off; }
  constexpr void initiate() { ++initiated; }
  constexpr void disable() { ++disabled; }
};
CTA_BEGIN_TESTS(myb_tests)
CTA_TEST(toggle_ui_ctx, ctx) {
  dummy_toggle t1;
  dummy_toggle t2;
  auto ui = ui_context::builder()
                .gpios(gpio_sel<1> >> std::ref(t1), gpio_sel<2> >> std::ref(t2))
                .build();
  ctx.expect_that(t1.toggle_count, eq(0));
  ctx.expect_that(t2.toggle_count, eq(0));
  ctx.expect_that(ui.toggle_gpio(1), eq(true));
  ctx.expect_that(t1.toggle_count, eq(1));
  ctx.expect_that(t2.toggle_count, eq(0));
  ctx.expect_that(ui.toggle_gpio(1), eq(true));
  ctx.expect_that(t1.toggle_count, eq(2));
  ctx.expect_that(t2.toggle_count, eq(0));
  ctx.expect_that(ui.toggle_gpio(2), eq(true));
  ctx.expect_that(t1.toggle_count, eq(2));
  ctx.expect_that(t2.toggle_count, eq(1));
  ctx.expect_that(ui.toggle_gpio(3), eq(false));
  ctx.expect_that(t1.toggle_count, eq(2));
  ctx.expect_that(t2.toggle_count, eq(1));
}
CTA_TEST(sleep_and_wake, ctx) {
  dummy_toggle t1;
  dummy_toggle t2;
  auto ui = ui_context::builder()
                .gpios(gpio_sel<1> >> std::ref(t1), gpio_sel<2> >> std::ref(t2))
                .build();
  ui.sleep();
  ctx.expect_that(t1.sleep_count, eq(1));
  ctx.expect_that(t2.sleep_count, eq(1));
  ctx.expect_that(t1.wake_count, eq(0));
  ctx.expect_that(t2.wake_count, eq(0));
  ui.wake();
  ctx.expect_that(t1.sleep_count, eq(1));
  ctx.expect_that(t2.sleep_count, eq(1));
  ctx.expect_that(t1.wake_count, eq(1));
  ctx.expect_that(t2.wake_count, eq(1));
  ui.sleep();
  ctx.expect_that(t1.sleep_count, eq(2));
  ctx.expect_that(t2.sleep_count, eq(2));
  ctx.expect_that(t1.wake_count, eq(1));
  ctx.expect_that(t2.wake_count, eq(1));
  ui.toggle_gpio(1);
  ctx.expect_that(t1.wake_count, eq(2));
  ctx.expect_that(t2.wake_count, eq(2));
}
CTA_TEST(led_raii_init_light_destruct, ctx) {
  dummy_output_pin pin;
  {
    auto led = led_wrap_pin(std::ref(pin));
    ctx.expect_that(pin.initiated, eq(1));
    ctx.expect_that(pin.disabled, eq(0));
    ctx.expect_that(pin.toggled_on, eq(0));
    ctx.expect_that(pin.toggled_off, eq(0));
    led.trigger();
    ctx.expect_that(pin.initiated, eq(1));
    ctx.expect_that(pin.disabled, eq(0));
    ctx.expect_that(pin.toggled_on, eq(1));
    ctx.expect_that(pin.toggled_off, eq(0));
    led.trigger();
    ctx.expect_that(pin.initiated, eq(1));
    ctx.expect_that(pin.disabled, eq(0));
    ctx.expect_that(pin.toggled_on, eq(1));
    ctx.expect_that(pin.toggled_off, eq(1));
    led.trigger();
    ctx.expect_that(pin.initiated, eq(1));
    ctx.expect_that(pin.disabled, eq(0));
    ctx.expect_that(pin.toggled_on, eq(2));
    ctx.expect_that(pin.toggled_off, eq(1));
  }
  ctx.expect_that(pin.initiated, eq(1));
  ctx.expect_that(pin.disabled, eq(1));
  ctx.expect_that(pin.toggled_on, eq(2));
  ctx.expect_that(pin.toggled_off, eq(2));
}
CTA_TEST(variant_behaviour_basics, ctx) {
  constexpr auto tot_states = 3;
  using array_t = std::array<int, tot_states>;
  array_t calls{};
  auto to_test = variant_stateless_function(
      std::ref(calls), [](array_t &c) { ++c[0]; }, [](array_t &c) { ++c[1]; },
      [](array_t &c) { ++c[2]; });
  ctx.expect_that(decltype(to_test)::size(), eq(3));
  to_test(calls);
  ctx.expect_that(calls[0], eq(1));
  ctx.expect_that(calls[1], eq(0));
  ctx.expect_that(calls[2], eq(0));
  ctx.expect_that(to_test.index(), eq(0));
  to_test.index(1);
  ctx.expect_that(to_test.index(), eq(1));
  to_test(calls);
  ctx.expect_that(calls[0], eq(1));
  ctx.expect_that(calls[1], eq(1));
  ctx.expect_that(calls[2], eq(0));
}
CTA_TEST(few_buttons_calculator_plus, ctx) {
  auto calc = few_buttons_calculator<3>();
  ctx.expect_that(calc.result(), eq(0u));
  calc.set_lhs(5);
  ctx.expect_that(calc.result(), eq(5u));
  calc.set_rhs(3);
  ctx.expect_that(calc.result(), eq(8u));
  calc.set_lhs(0b111);
  calc.set_rhs(0b111);
  ctx.expect_that(calc.result(), eq(0b111 + 0b111));
}
CTA_TEST(few_buttons_calculator_minus, ctx) {
  auto calc = few_buttons_calculator<3>();
  calc.set_operator(few_buttons_calculator_operations::subtract);
  calc.set_lhs(5);
  calc.set_rhs(3);
  ctx.expect_that(calc.result(), eq(2u));
  calc.set_rhs(8);
  ctx.expect_that(calc.result(), eq(0b111111 - 2u));
  calc.swap_lr();
  ctx.expect_that(calc.result(), eq(3));
}
CTA_TEST(few_buttons_calculator_multiply, ctx) {
  auto calc = few_buttons_calculator<3>();
  calc.set_operator(few_buttons_calculator_operations::multiply);
  calc.set_lhs(5);
  calc.set_rhs(3);
  ctx.expect_that(calc.result(), eq(15u));
  calc.set_lhs(0b111);
  calc.set_rhs(0b111);
  ctx.expect_that(calc.result(), eq(0b111 * 0b111));
}
CTA_TEST(few_buttons_calculator_divide, ctx) {
  auto calc = few_buttons_calculator<3>();

  auto div_expect = [&ctx, &calc](auto quot, auto rem,
                                  cta::etd::source_location const &sl =
                                      cta::etd::source_location::current()) {
    auto concat_result = (quot << 3) | rem;
    ctx.expect_that(calc.result(), eq(concat_result), sl);
  };
  calc.set_operator(few_buttons_calculator_operations::divide);
  calc.set_lhs(5);
  calc.set_rhs(3);
  div_expect(1, 2);
  calc.set_lhs(0b111);
  calc.set_rhs(0b111);
  div_expect(1, 0);
  calc.set_rhs(0);
  ctx.expect_that(calc.can_compute(), eq(false));
  calc.result();
}
CTA_END_TESTS()
} // namespace myb

int main() {
#ifdef MYB_PICO
#define MYB_TEST_RETURN(RES)
  stdio_init_all();
  while (true) {
    if (stdio_usb_connected()) {
      fmt::print("USB Connected!\n");
      break;
    }
  }
#else
#define MYB_TEST_RETURN(RES) return RES;
#endif

  using namespace std::chrono;

  std::cout << "Run tests...\n";
  auto cur_time = steady_clock::now();
  auto test_result = cta::just_run_tests();
  auto end_time = steady_clock::now();
  std::cout << "Ran " << test_result.total_tests << " tests in "
            << duration_cast<microseconds>(end_time - cur_time) << '\n';
  if (test_result.total_tests == 0) {
    std::cout << "WARNING: No tests run\n";
  }
  fmt::print("Tests run/passed/failed: {}/{}/{}\n", test_result.total_tests,
             test_result.total_tests - test_result.failed, test_result.failed);
  if (failed(test_result)) {
    std::cout << "Failed tests found\n";
    MYB_TEST_RETURN(EXIT_FAILURE)
  } else {
    std::cout << "All tests passed\n";
    MYB_TEST_RETURN(EXIT_SUCCESS)
  }
#ifdef MYB_PICO
  while (tud_cdc_n_write_flush(0) != 0) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  while (stdio_usb_connected()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  tud_disconnect();
  reset_usb_boot(0, 0);
#endif
}
