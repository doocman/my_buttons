
#include <array>
#include <bitset>
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
struct dummy_all_pin_cb {
  int calls_1{};
  int calls_2{};
  int calls_err{};
  constexpr void operator()(uint i) {
    switch (i) {
    case 1u:
      ++calls_1;
      return;
    case 2u:
      ++calls_2;
      return;
    default:
      ++calls_err;
      return;
    }
  }
};
struct dummy_calc_out {
  using in_set = std::bitset<3>;
  using res_set = std::bitset<6>;
  using op_set = std::bitset<2>;
  in_set lhs{};
  in_set rhs{};
  res_set result{};
  op_set op{};
  bool no_result_{};

  constexpr void set_lhs(in_set v) { lhs = v; }
  constexpr void set_rhs(in_set v) { rhs = v; }
  constexpr void set_result(res_set v) {
    result = v;
    no_result_ = false;
  }
  constexpr void set_no_result() { no_result_ = true; }
  constexpr void set_operator(op_set v) { op = v; }
};
struct dummy_redyelgreen_out {
  bool red_on{};
  bool yel_on{};
  bool gre_on{};

  constexpr void red(bool v) { red_on = v; };
  constexpr void yellow(bool v) { yel_on = v; };
  constexpr void green(bool v) { gre_on = v; };
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
  ctx.expect_that(ui.trigger_gpio(1), eq(true));
  ctx.expect_that(t1.toggle_count, eq(1));
  ctx.expect_that(t2.toggle_count, eq(0));
  ctx.expect_that(ui.trigger_gpio(1), eq(true));
  ctx.expect_that(t1.toggle_count, eq(2));
  ctx.expect_that(t2.toggle_count, eq(0));
  ctx.expect_that(ui.trigger_gpio(2), eq(true));
  ctx.expect_that(t1.toggle_count, eq(2));
  ctx.expect_that(t2.toggle_count, eq(1));
  ctx.expect_that(ui.trigger_gpio(3), eq(false));
  ctx.expect_that(t1.toggle_count, eq(2));
  ctx.expect_that(t2.toggle_count, eq(1));
}
CTA_TEST(ui_ctx_callback, ctx) {
  int callbacked{};
  dummy_toggle t1;
  dummy_toggle t2;
  auto ui = ui_context::builder()
                .gpios(gpio_sel<1> >> std::ref(t1), gpio_sel<2> >> std::ref(t2))
                .build();
  auto cb_fun = [&callbacked] { ++callbacked; };
  ctx.expect_that(ui.trigger_gpio(1, cb_fun), eq(true));
  ctx.expect_that(callbacked, eq(1));
  ctx.expect_that(ui.trigger_gpio(2, cb_fun), eq(true));
  ctx.expect_that(callbacked, eq(2));
  ctx.expect_that(ui.trigger_gpio(3, cb_fun), eq(false));
  ctx.expect_that(callbacked, eq(2));
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
}
CTA_TEST(all_inputs_in_ctx, ctx) {
  auto pins_cb = dummy_all_pin_cb{};
  dummy_toggle t1;
  dummy_toggle t2;
  auto ui = ui_context::builder()
                .gpios(gpio_sel<1> >> std::ref(t1), gpio_sel<2> >> std::ref(t2))
                .build();
  ui.for_each_input(pins_cb);
  ctx.expect_that(pins_cb.calls_1, eq(1));
  ctx.expect_that(pins_cb.calls_2, eq(1));
  ctx.expect_that(pins_cb.calls_err, eq(0));
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
  ctx.expect_that(calc.current_operator(),
                  eq(few_buttons_calculator_operations::add));
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
  ctx.expect_that(calc.current_operator(),
                  eq(few_buttons_calculator_operations::subtract));
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
  ctx.expect_that(calc.current_operator(),
                  eq(few_buttons_calculator_operations::multiply));
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
  ctx.expect_that(calc.current_operator(),
                  eq(few_buttons_calculator_operations::divide));
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
CTA_TEST(few_buttons_calculator_rotate, ctx) {
  auto calc = few_buttons_calculator<3>();
  ctx.expect_that(calc.current_operator(),
                  eq(few_buttons_calculator_operations::add));
  rotate_inplace(&calc);
  ctx.expect_that(calc.current_operator(),
                  eq(few_buttons_calculator_operations::subtract));
  rotate_inplace(&calc);
  ctx.expect_that(calc.current_operator(),
                  eq(few_buttons_calculator_operations::multiply));
  rotate_inplace(&calc);
  ctx.expect_that(calc.current_operator(),
                  eq(few_buttons_calculator_operations::divide));
  rotate_inplace(&calc);
  ctx.expect_that(calc.current_operator(),
                  eq(few_buttons_calculator_operations::add));
}
CTA_TEST(calc_2_led_tests, ctx) {
  auto calc = few_buttons_calculator<3>();
  auto calc_fetch = [&calc]() -> auto & { return calc; };
  auto calc_wrapper = calc_2_led(calc_fetch);
  auto led_out = dummy_calc_out{};
  using in_set = dummy_calc_out::in_set;
  using res_set = dummy_calc_out::res_set;
  using op_set = dummy_calc_out::op_set;
  calc_wrapper.template toggle_bit<0>(led_out);
  ctx.expect_that(calc.rhs(), eq(1));
  ctx.expect_that(calc.lhs(), eq(0));
  ctx.expect_that(calc.current_operator(),
                  eq(few_buttons_calculator_operations::add));
  ctx.expect_that(led_out.op, eq(op_set(0ull)));
  ctx.expect_that(led_out.rhs, eq(in_set(1ull)));
  ctx.expect_that(led_out.lhs, eq(in_set(0ull)));
  ctx.expect_that(led_out.result, eq(res_set(0b000001ull)));
  ctx.expect_that(led_out.no_result_, eq(false));
  calc_wrapper.template toggle_bit<0>(led_out);
  ctx.expect_that(calc.rhs(), eq(0));
  ctx.expect_that(calc.lhs(), eq(0));
  ctx.expect_that(calc.current_operator(),
                  eq(few_buttons_calculator_operations::add));
  ctx.expect_that(led_out.op, eq(op_set("00")));
  ctx.expect_that(led_out.rhs, eq(in_set("000")));
  ctx.expect_that(led_out.lhs, eq(in_set("000")));
  ctx.expect_that(led_out.result, eq(res_set("000000")));
  ctx.expect_that(led_out.no_result_, eq(false));
  calc_wrapper.template toggle_bit<1>(led_out);
  ctx.expect_that(calc.rhs(), eq(2));
  ctx.expect_that(calc.lhs(), eq(0));
  ctx.expect_that(calc.current_operator(),
                  eq(few_buttons_calculator_operations::add));
  ctx.expect_that(led_out.op, eq(op_set("00")));
  ctx.expect_that(led_out.rhs, eq(in_set("010")));
  ctx.expect_that(led_out.lhs, eq(in_set("000")));
  ctx.expect_that(led_out.result, eq(res_set("000010")));
  ctx.expect_that(led_out.no_result_, eq(false));
  calc_wrapper.rotate_behaviour(led_out);
  ctx.expect_that(calc.rhs(), eq(0));
  ctx.expect_that(calc.lhs(), eq(2));
  ctx.expect_that(calc.current_operator(),
                  eq(few_buttons_calculator_operations::add));
  ctx.expect_that(led_out.op, eq(op_set("00")));
  ctx.expect_that(led_out.rhs, eq(in_set("000")));
  ctx.expect_that(led_out.lhs, eq(in_set("010")));
  ctx.expect_that(led_out.result, eq(res_set("000010")));
  ctx.expect_that(led_out.no_result_, eq(false));
  calc_wrapper.template toggle_bit<2>(led_out);
  ctx.expect_that(calc.rhs(), eq(4));
  ctx.expect_that(calc.lhs(), eq(2));
  ctx.expect_that(calc.current_operator(),
                  eq(few_buttons_calculator_operations::add));
  ctx.expect_that(led_out.op, eq(op_set("00")));
  ctx.expect_that(led_out.rhs, eq(in_set("100")));
  ctx.expect_that(led_out.lhs, eq(in_set("010")));
  ctx.expect_that(led_out.result, eq(res_set("000110")));
  ctx.expect_that(led_out.no_result_, eq(false));
  calc_wrapper.rotate_behaviour(led_out);
  ctx.expect_that(calc.rhs(), eq(4));
  ctx.expect_that(calc.lhs(), eq(2));
  ctx.expect_that(calc.current_operator(),
                  eq(few_buttons_calculator_operations::add));
  ctx.expect_that(led_out.op, eq(op_set("00")));
  ctx.expect_that(led_out.rhs, eq(in_set("100")));
  ctx.expect_that(led_out.lhs, eq(in_set("010")));
  ctx.expect_that(led_out.result, eq(res_set("000110")));
  ctx.expect_that(led_out.no_result_, eq(false));
  calc_wrapper.template toggle_bit<0>(led_out);
  ctx.expect_that(calc.rhs(), eq(4));
  ctx.expect_that(calc.lhs(), eq(2));
  ctx.expect_that(calc.current_operator(),
                  eq(few_buttons_calculator_operations::subtract));
  ctx.expect_that(led_out.op, eq(op_set("01")));
  ctx.expect_that(led_out.rhs, eq(in_set("100")));
  ctx.expect_that(led_out.lhs, eq(in_set("010")));
  ctx.expect_that(led_out.result, eq(res_set(static_cast<unsigned long>(-2))));
  ctx.expect_that(led_out.no_result_, eq(false));
  calc_wrapper.template toggle_bit<1>(led_out);
  ctx.expect_that(calc.rhs(), eq(4));
  ctx.expect_that(calc.lhs(), eq(2));
  ctx.expect_that(calc.current_operator(),
                  eq(few_buttons_calculator_operations::divide));
  ctx.expect_that(led_out.op, eq(op_set("11")));
  ctx.expect_that(led_out.rhs, eq(in_set("100")));
  ctx.expect_that(led_out.lhs, eq(in_set("010")));
  ctx.expect_that(led_out.result, eq(res_set("000010")));
  ctx.expect_that(led_out.no_result_, eq(false));
  calc_wrapper.rotate_behaviour(led_out);
  ctx.expect_that(calc.rhs(), eq(4));
  ctx.expect_that(calc.lhs(), eq(2));
  ctx.expect_that(calc.current_operator(),
                  eq(few_buttons_calculator_operations::divide));
  ctx.expect_that(led_out.op, eq(op_set("11")));
  ctx.expect_that(led_out.rhs, eq(in_set("100")));
  ctx.expect_that(led_out.lhs, eq(in_set("010")));
  ctx.expect_that(led_out.result, eq(res_set("000010")));
  ctx.expect_that(led_out.no_result_, eq(false));
  calc_wrapper.template toggle_bit<0>(led_out);
  ctx.expect_that(calc.rhs(), eq(5));
  ctx.expect_that(calc.lhs(), eq(2));
  calc_wrapper.rotate_behaviour(led_out);
  ctx.expect_that(calc.rhs(), eq(2));
  ctx.expect_that(calc.lhs(), eq(5));
  ctx.expect_that(calc.current_operator(),
                  eq(few_buttons_calculator_operations::divide));
  ctx.expect_that(led_out.op, eq(op_set("11")));
  ctx.expect_that(led_out.rhs, eq(in_set(2u)));
  ctx.expect_that(led_out.lhs, eq(in_set(5u)));
  ctx.expect_that(led_out.result, eq(res_set("010001")));
  ctx.expect_that(led_out.no_result_, eq(false));

  calc_wrapper.template toggle_bit<1>(led_out);
  ctx.expect_that(calc.rhs(), eq(0));
  ctx.expect_that(calc.lhs(), eq(5));
  ctx.expect_that(calc.can_compute(), eq(false));
  ctx.expect_that(led_out.no_result_, eq(true));
}
CTA_TEST(typed_time_queue_basics, ctx) {
  int val_a{};
  int val_b{};
  auto cb_a = [&val_a](auto &&...) { ++val_a; };
  auto cb_b = [&val_b](auto &&...) { ++val_b; };
  using namespace std::chrono;
  using clock_t = steady_clock;
  using time_point = clock_t::time_point;
  auto to_test = typed_time_queue(time_point{}, cb_a, cb_b);
  auto next = to_test.next();
  ctx.expect_that(next, eq(std::nullopt));
  ctx.expect_that(to_test.execute_all(time_point{}), eq(0));
  to_test.que(cb_a, time_point(1ns));
  ctx.expect_that(to_test.next(), eq(time_point(1ns)));
  ctx.expect_that(to_test.execute_all(time_point(1ns)), eq(1));
  ctx.expect_that(val_a, eq(1));
  ctx.expect_that(val_b, eq(0));
  ctx.expect_that(to_test.execute_all(time_point(1ns)), eq(0));
  ctx.expect_that(val_a, eq(1));
  ctx.expect_that(val_b, eq(0));
  to_test.que(cb_a, time_point(2ns));
  to_test.que(cb_a, time_point(3ns));
  ctx.expect_that(to_test.execute_all(time_point(4ns)), eq(1));
  ctx.expect_that(val_a, eq(2));
  ctx.expect_that(val_b, eq(0));
  to_test.que(cb_a, time_point(4ns));
  to_test.que(cb_b, time_point(4ns));
  ctx.expect_that(to_test.execute_all(time_point(4ns)), eq(2));
  ctx.expect_that(val_a, eq(3));
  ctx.expect_that(val_b, eq(1));
}
CTA_TEST(typed_time_queue_unque, ctx) {
  int val_a{};
  int val_b{};
  auto cb_a = [&val_a](auto &&...) { ++val_a; };
  auto cb_b = [&val_b](auto &&...) { ++val_b; };
  using namespace std::chrono;
  using clock_t = steady_clock;
  using time_point = clock_t::time_point;
  auto to_test = typed_time_queue(time_point{}, cb_a, cb_b);
  to_test.que(cb_a, time_point(2ns));
  to_test.que(cb_b, time_point(3ns));
  to_test.unque(cb_a);
  ctx.expect_that(to_test.next(), eq(time_point(3ns)));
  ctx.expect_that(to_test.execute_all(time_point(3ns)), eq(1));
  ctx.expect_that(val_a, eq(0));
  ctx.expect_that(val_b, eq(1));
}
CTA_TEST(traffic_light_basics, ctx) {
  auto out = dummy_redyelgreen_out();
  auto to_test = traffic_light_fsm();
  to_test.write_to(out);
  ctx.expect_that(out.red_on, eq(true));
  ctx.expect_that(out.yel_on, eq(false));
  ctx.expect_that(out.gre_on, eq(false));
  to_test.advance();
  to_test.write_to(out);
  ctx.expect_that(out.red_on, eq(true));
  ctx.expect_that(out.yel_on, eq(true));
  ctx.expect_that(out.gre_on, eq(false));
  to_test.advance();
  to_test.write_to(out);
  ctx.expect_that(out.red_on, eq(false));
  ctx.expect_that(out.yel_on, eq(false));
  ctx.expect_that(out.gre_on, eq(true));
  to_test.advance();
  to_test.write_to(out);
  ctx.expect_that(out.red_on, eq(false));
  ctx.expect_that(out.yel_on, eq(true));
  ctx.expect_that(out.gre_on, eq(false));
  to_test.advance();
  to_test.write_to(out);
  ctx.expect_that(out.red_on, eq(true));
  ctx.expect_that(out.yel_on, eq(false));
  ctx.expect_that(out.gre_on, eq(false));
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
