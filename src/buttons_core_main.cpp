
#include <algorithm>
#include <array>
#include <chrono>
#include <thread>

#include "pico/stdlib.h"
#include <hardware/adc.h>
#include <hardware/dma.h>
#include <hardware/pwm.h>

#include <picolinux/picolinux_libc.hpp>

#include <app/myb_app.hpp>
#include <myb/myb.hpp>

#if __has_include(<class/cdc/cdc_device.h>)
#define DEBUG 1
#include <class/cdc/cdc_device.h>
#include <device/usbd.h>
#include <fmt/chrono.h>
#include <fmt/core.h>
#include <pico/bootrom.h>
#include <pico/stdlib.h>
#else
#define DEBUG 0
#endif

namespace myb {

template <ct_int pin> struct pico_toggle_gpio {
  static void on_sleep() { gpio_put(pin.i, 0u); }
  static void on_wake() {
    gpio_init(pin.i);
    gpio_set_dir(pin.i, GPIO_OUT);
  }
  static void trigger() {
    auto cur_val = gpio_get(pin.i);
    gpio_put(pin.i, cur_val == 0 ? 1u : 0u);
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
    dma_channel_set_irq0_enabled(dma_chan_, true);
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
    dma_hw->ints0 = 1u << dma_chan_;
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

using steady_clock = std::chrono::steady_clock;
inline constexpr auto sleep_timeout = std::chrono::minutes(5);
inline constexpr auto sleep_poll_timeout = std::chrono::seconds(5);
static auto next_sleep = steady_clock::time_point{};
static auto wake_other = rxtx_wake_interrupt<wake_tx_gpio>();

static auto timed_queue = typed_time_queue(
    steady_clock::time_point{}, call_static_reset<decltype(wake_other)>{});

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

void wake_and_prolong_no_send(steady_clock::time_point now) {
  wake_other.init();
  next_sleep = now + sleep_timeout;
}
void wake_and_prolong_no_send() {
  wake_and_prolong_no_send(steady_clock::now());
}
void wake_and_prolong() {
  wake_and_prolong_no_send();
  wake_other.set(timed_queue);
}
void sleep() {
  context.sleep();
  the_adc.sleep();
  the_fader_t::sleep();
  go_deep_sleep();
}

void gpio_irq(uint gpio, std::uint32_t events) {
  constexpr std::uint32_t edge_rise_mask = 0b1000u;
  // We only care about edge rise.
  if ((events & edge_rise_mask) == 0) {
    return;
  }
  if (gpio == 7) {
    wake_and_prolong_no_send();
  } else {
    context.trigger_gpio(gpio, [] { wake_and_prolong(); });
  }
}
static auto old_adc_value = decltype(the_adc.read_averaged_adc()){};

void dma_irq() {
  auto v = the_adc.read_averaged_adc();
#if DEBUG
  fmt::print("Averaged ADC value is {}\n", v);
#endif
  if (static_cast<unsigned>(v - old_adc_value) >= 32) {
    next_sleep = steady_clock::now() + sleep_timeout;
    old_adc_value = v;
  }
  the_fader_t::set_level(v >> 4);
}

void main() {
  // setup_adc();
  while (1) {

#if DEBUG
    stdio_init_all();
    while (true) {
      if (stdio_usb_connected()) {
        fmt::print("USB Connected!\n");
        break;
      }
    }
#endif
    gpio_set_dir(wake_rx_gpio, GPIO_IN);
    gpio_set_irq_enabled_with_callback(wake_rx_gpio, GPIO_IRQ_EDGE_RISE, true,
                                       &gpio_irq);
    gpio_set_dormant_irq_enabled(wake_rx_gpio, GPIO_IRQ_EDGE_RISE, true);
    context.for_each_input([](uint pin) {
      gpio_set_dir(pin, GPIO_IN);
      gpio_set_irq_enabled(pin, GPIO_IRQ_EDGE_RISE, true);
      gpio_set_dormant_irq_enabled(pin, GPIO_IRQ_EDGE_RISE, true);
    });
    irq_set_exclusive_handler(DMA_IRQ_0, &dma_irq);
    irq_set_enabled(DMA_IRQ_0, true);
    the_adc.init();
    the_fader_t::init();

#if DEBUG
    while (stdio_usb_connected()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
      if (steady_clock::now() >= next_sleep) {
        sleep();
      }
    }
    irq_set_enabled(DMA_IRQ_0, false);
    while (tud_cdc_n_write_flush(0) != 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    tud_disconnect();
    reset_usb_boot(0, 0);
#else
    while (steady_clock::now() < next_sleep) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    sleep();
#endif
  }
}
} // namespace myb

int main() {
  while (1) {
    myb::main();
  }
}
