

#include <chrono>
#include <thread>

#include <class/cdc/cdc_device.h>
#include <device/usbd.h>
#include <pico/bootrom.h>
#include <pico/stdlib.h>

#include <fmt/core.h>

#include <picolinux/picolinux_libc.hpp>

#include <myb/myb.hpp>

#include <cta/cta.hpp>

#define MYB_PICO

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

  auto test_result = cta::just_run_tests();
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
