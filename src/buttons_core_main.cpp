
#include <chrono>
#include <thread>

#include "pico/stdlib.h"
#include <hardware/pwm.h>

#include <picolinux/picolinux_libc.hpp>

#include <myb/myb.hpp>

#include <class/cdc/cdc_device.h>
#include <device/usbd.h>
#include <fmt/core.h>
#include <pico/bootrom.h>
#include <pico/stdlib.h>

namespace myb {

template <ct_int pin> struct pico_toggle_gpio {
  static void on_sleep() { gpio_set(pin.i, 0u); }
  static void on_wake() {
    gpio_init(pin.i);
    gpio_set_dir(pin.i, GPIO_OUT);
  }
  static void trigger() {
    auto cur_val = gpio_get(pin.i);
    gpio_set(pin.i, cur_val == 0 ? 1u : 0u);
  }
};

// gpio 0+1 -> i2c to other RPi Pico
// gpio 2,3,4,5 -> reserved for future SPIO or i2c.
// gpio 6 -> send wake interrupt
// gpio 7 -> receive wake interrupt
// gpio 26 -> ADC for potentiometer

inline constexpr uint wake_tx_gpio = 6u;
inline constexpr uint wake_rx_gpio = 7u;

static auto context = ui_context::builder()
                          .gpios(                                     //
                              gpio_sel<8> >> pico_toggle_gpio<9>(),   //> red
                              gpio_sel<10> >> pico_toggle_gpio<11>(), //> green
                              gpio_sel<12> >> pico_toggle_gpio<13>()  //> blue
                              //
                              )
                          .build();

void main() {
  stdio_init_all();
  while (true) {
    if (stdio_usb_connected()) {
      break;
    }
  }

  adc_init();

  // Make sure GPIO is high-impedance, no pullups etc
  adc_gpio_init(26);
  // Select ADC input 0 (GPIO26)
  adc_select_input(0);

  gpio_set_function(25, GPIO_FUNC_PWM);

  // Find out which PWM slice is connected to GPIO 25
  uint slice_num = pwm_gpio_to_slice_num(25);
  uint channel = pwm_gpio_to_channel(25);

  // Set period of 4 cycles (0 to 3 inclusive)
  pwm_set_wrap(slice_num, 255);
  // Set channel A output high for one cycle before dropping
  pwm_set_chan_level(slice_num, channel, 1);
  // Set the PWM running
  pwm_set_enabled(slice_num, true);

  while (tud_cdc_n_write_flush(0) != 0) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  int cur_pwm_level = 1;
  while (stdio_usb_connected()) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    ++cur_pwm_level;
    cur_pwm_level = cur_pwm_level % 256;
    pwm_set_chan_level(slice_num, channel, cur_pwm_level);
    // std::cout << "set pwm level " << cur_pwm_level << '\n';
    fmt::print("Set pwm level to {}\n", cur_pwm_level);
  }
  tud_disconnect();
  reset_usb_boot(0, 0);
}
} // namespace myb

int main() {
  while (1) {
    myb::main();
  }
}
