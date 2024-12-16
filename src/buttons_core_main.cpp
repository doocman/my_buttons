
#include <algorithm>
#include <array>
#include <chrono>
#include <thread>

#include "pico/stdlib.h"
#include <hardware/adc.h>
#include <hardware/dma.h>
#include <hardware/pwm.h>

#include <picolinux/picolinux_libc.hpp>

#include <myb/myb.hpp>

#include <class/cdc/cdc_device.h>
#include <device/usbd.h>
#include <fmt/chrono.h>
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

template <ct_int adc_pin, std::size_t buff_size> class adc2dma {
  std::array<std::uint16_t, buff_size * 2> tot_buff_;
  uint dma_chan_;
  dma_channel_config cfg_;
  bool read_first_ = false;

  static constexpr auto adc_channel = adc_pin.i - 26;

  template <bool is_read>
  constexpr std::span<std::uint16_t, buff_size> get_buff() {
    if (is_read == read_first_) {
      return std::span<std::uint16_t, buff_size>{tot_buff_.data(), buff_size};
    } else {
      return std::span<std::uint16_t, buff_size>{
          tot_buff_.data() + static_cast<std::ptrdiff_t>(buff_size), buff_size};
    }
  }

  constexpr std::span<std::uint16_t, buff_size> write_buff() {
    return get_buff<false>();
  }
  constexpr std::span<std::uint16_t, buff_size> read_buff() {
    return get_buff<true>();
  }

public:
  // simply because the buffer must be in memory. Could be other ways to do
  // this.
  adc2dma(adc2dma const &) = delete;
  adc2dma &operator=(adc2dma const &) = delete;
  adc2dma() = default;

  void init() {

    adc_init();

    // Make sure GPIO is high-impedance, no pullups etc
    adc_gpio_init(adc_pin.i);
    // Select ADC input 0 (GPIO26)
    adc_select_input(adc_channel);
    adc_set_clkdiv(0);
    adc_fifo_setup(
        true,  // Write each completed conversion to the sample FIFO
        true,  // Enable DMA data request (DREQ)
        1,     // DREQ (and IRQ) asserted when at least 1 sample present
        false, // We won't see the ERR bit because of 8 bit reads; disable.
        false  // Shift each sample to 8 bits when pushing to FIFO
    );

    dma_chan_ = dma_claim_unused_channel(true);
    cfg_ = dma_channel_get_default_config(dma_chan_);

    // Reading from constant address, writing to incrementing byte addresses
    channel_config_set_transfer_data_size(&cfg_, DMA_SIZE_16);
    channel_config_set_read_increment(&cfg_, false);
    channel_config_set_write_increment(&cfg_, true);

    // Pace transfers based on availability of ADC samples
    channel_config_set_dreq(&cfg_, DREQ_ADC);

    auto capture_buf = write_buff();
    dma_channel_configure(dma_chan_, &cfg_,
                          capture_buf.data(), // dst
                          &adc_hw->fifo,      // src
                          capture_buf.size(), // transfer count
                          true                // start immediately
    );
    adc_run(true);
  }
  bool is_adc_ready() { return !dma_channel_is_busy(dma_chan_); }
  auto read_averaged_adc() {
    read_first_ = !read_first_;
    auto to_read = read_buff();
    auto capture_buf = write_buff();
    dma_channel_configure(dma_chan_, &cfg_,
                          capture_buf.data(), // dst
                          &adc_hw->fifo,      // src
                          capture_buf.size(), // transfer count
                          true                // start immediately
    );
    auto res_sum =
        std::ranges::fold_left(to_read, std::int_fast32_t{}, std::plus<>{});
    return res_sum / buff_size;
  }
  void sleep() {
    adc_run(false);
    adc_fifo_drain();
  }
};

template <ct_int pin, uint resolution>
  requires(resolution > 1)
class pwm_led_fader {
public:
  static void init() {
    gpio_set_function(pin.i, GPIO_FUNC_PWM);

    // Find out which PWM slice is connected to GPIO pin
    uint slice_num = pwm_gpio_to_slice_num(pin.i);
    uint channel = pwm_gpio_to_channel(pin.i);

    // Set period of resolution cycles (is inclusive)
    pwm_set_wrap(slice_num, resolution - 1);
    // Set channel A output high for one cycle before dropping
    pwm_set_chan_level(slice_num, channel, 1);
    // Set the PWM running
    pwm_set_enabled(slice_num, true);
  }
  static void set_level(uint l) {
    uint slice_num = pwm_gpio_to_slice_num(pin.i);
    uint channel = pwm_gpio_to_channel(pin.i);
    pwm_set_chan_level(slice_num, channel, l);
  }
  static void sleep() {
    uint slice_num = pwm_gpio_to_slice_num(pin.i);
    pwm_set_enabled(slice_num, false);
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

static auto the_adc = adc2dma<26, 512>{};
using the_fader_t = pwm_led_fader<25, 256>;

void main() {
  stdio_init_all();
  while (true) {
    if (stdio_usb_connected()) {
      break;
    }
  }

  // setup_adc();
  the_adc.init();
  the_fader_t::init();

  while (tud_cdc_n_write_flush(0) != 0) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  int cur_pwm_level = 1;
  using namespace std::chrono;
  auto now = steady_clock::now();
  auto next_print = now + 1s;
  int num_adc_vals{};
  int adc_sum{};
  nanoseconds time_waited{};
  nanoseconds time_calculated{};
  while (stdio_usb_connected()) {
    // std::this_thread::sleep_for(std::chrono::seconds(1));
    auto org_tp = steady_clock::now();
    // wait_for_adc();
    while (!the_adc.is_adc_ready()) {
    }
    auto wait_end = steady_clock::now();
    adc_sum += the_adc.read_averaged_adc();
    ++num_adc_vals;
    now = steady_clock::now();
    time_waited += (wait_end - org_tp);
    time_calculated += (now - wait_end);
    if (now >= next_print) {
      ++cur_pwm_level;
      cur_pwm_level = cur_pwm_level % 256;
      the_fader_t::set_level(cur_pwm_level);
      fmt::print("Set pwm level to {}\n", cur_pwm_level);
      if (num_adc_vals != 0) {
        fmt::print("ADC value was {}\n", adc_sum / num_adc_vals);
        adc_sum = 0;
        num_adc_vals = 0;
      } else {
        fmt::print("NO ADC VALUE WAS RECEIVED!\n");
      }
      fmt::print("Time waited: {} and time calculated: {}\n", time_waited,
                 time_calculated);
      time_waited = 0ns;
      time_calculated = 0ns;
      next_print = now + 1s;
    }
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
