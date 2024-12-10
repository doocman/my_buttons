
#include <chrono>
#include <functional>
#include <thread>
#include <type_traits>

#include "pico/stdlib.h"

#include <fmt/core.h>

#include <picolinux/picolinux_libc.hpp>

#include <myb/myb.hpp>

template <typename Fetcher>
  requires(std::is_empty_v<Fetcher> && std::invocable<Fetcher> &&
           std::is_lvalue_reference_v<std::invoke_result_t<Fetcher>>)
struct static_toggle_wrapper : private Fetcher {
  constexpr decltype(auto) trigger() noexcept { return Fetcher::trigger(); }
  constexpr decltype(auto) on_sleep() noexcept { return Fetcher::on_sleep(); }
  constexpr decltype(auto) on_wake() noexcept { return Fetcher::on_wake(); }
};

namespace myb {
inline namespace {
int main() {
  while (1) {
  }
}
} // namespace
} // namespace myb

int main() { return myb::main(); }
