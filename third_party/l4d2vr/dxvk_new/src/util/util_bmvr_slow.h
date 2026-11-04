#pragma once
#include <atomic>
#include <chrono>
#include "log/log.h"
#include "util_string.h"
#include "util_env.h"
#include "util_bmvr_timing.h"
namespace dxvk {
// No frame-by-frame output or GPU synchronization. Only unusually slow calls.
class BmvrSlowScope {
  bool m_enabled;
  const char* m_name;
  unsigned m_tag;
  std::atomic<unsigned>& m_budget;
  std::chrono::steady_clock::time_point m_start{};
public:
  BmvrSlowScope(const char* name, std::atomic<unsigned>& budget)
  : m_enabled(enabled()), m_name(name), m_tag(m_enabled ? BmvrTimingTag(name) : BmvrTimingCount), m_budget(budget) {
    if (m_enabled) m_start=std::chrono::steady_clock::now();
  }
  static bool enabled() {
    static const bool value=env::getEnvVar("BMVR_PERF_DIAG")=="1" || env::getEnvVar("BMVR_STALL_CAPTURE")=="1";
    return value;
  }
  ~BmvrSlowScope() {
    if (!m_enabled) return;
    const auto elapsed = std::chrono::steady_clock::now() - m_start;
    const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();
    BmvrTimingAdd(m_tag,static_cast<uint64_t>(ns));
    const double ms = double(ns) / 1000000.0;
    if (ms >= 1000.0 && m_budget.fetch_add(1, std::memory_order_relaxed) < 32) {
      // Diagnostics must remain optional even during Win32 allocation pressure.
      try { Logger::warn(str::format("[BMVR slow] ", m_name, " durationMs=", ms)); }
      catch (...) { }
    }
  }
};
}
