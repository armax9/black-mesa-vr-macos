#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#ifdef _WIN32
#include <Windows.h>
#endif
#include "log/log.h"
#include "util_string.h"
#include "util_bmvr_timing.h"

namespace dxvk {
inline bool BmvrCpuMemoryEnabled() {
  static const bool enabled = [] {
    const char* value = std::getenv("BMVR_STALL_CAPTURE");
    const char* perf = std::getenv("BMVR_PERF_DIAG");
    if (perf && perf[0]=='1' && perf[1]=='\0') return true;
    return value && value[0] == '1' && value[1] == '\0';
  }();
  return enabled;
}
inline bool BmvrCpuMemoryClaimSlowEvent() {
  // One cap across all call sites/TUs in this DLL, without counter wrap.
  static std::atomic<unsigned> events{0};
  unsigned observed = events.load(std::memory_order_relaxed);
  while (observed < 12) {
    if (events.compare_exchange_weak(observed, observed + 1,
          std::memory_order_relaxed, std::memory_order_relaxed)) return true;
  }
  return false;
}
// Place immediately around an actual CPU copy/clear/pack. Disabled launches
// take no clocks; enabled operations take exactly two steady-clock readings.
class BmvrCpuMemoryScope {
  unsigned m_tag;
  uint64_t m_bytes;
  const void* m_dst;
  const void* m_src;
  bool m_enabled;
  std::chrono::steady_clock::time_point m_start{};
public:
  BmvrCpuMemoryScope(unsigned tag, uint64_t bytes, const void* dst,
      const void* src = nullptr) noexcept
  : m_tag(tag), m_bytes(bytes), m_dst(dst), m_src(src),
    m_enabled(tag >= BmvrCpuConstantCopy && tag < BmvrTimingCount
      && BmvrCpuMemoryEnabled()) {
    if (m_enabled) m_start = std::chrono::steady_clock::now();
  }
  BmvrCpuMemoryScope(const BmvrCpuMemoryScope&) = delete;
  BmvrCpuMemoryScope& operator=(const BmvrCpuMemoryScope&) = delete;
  ~BmvrCpuMemoryScope() noexcept {
    if (!m_enabled) return;
    const auto stop = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(stop - m_start).count();
    const uint64_t ns = elapsed > 0 ? static_cast<uint64_t>(elapsed) : 0;
    BmvrTimingAdd(m_tag, ns);
    if (ns <= 50000000u || !BmvrCpuMemoryClaimSlowEvent()) return;
    // Read thread/tick and format/log only after the measured memory operation.
    // No GPU calls, memory queries, explicit allocator locks or data accesses.
    try {
#ifdef _WIN32
      const DWORD thread = GetCurrentThreadId();
      const uint64_t tick = GetTickCount64();
      Logger::warn(str::format("[BMVR CPU memory] tag=", BmvrTimingName(m_tag),
        " bytes=", m_bytes, " src=", m_src, " dst=", m_dst,
        " durationMs=", double(ns) / 1000000.0,
        " thread=", thread, " tick=", tick));
#else
      Logger::warn(str::format("[BMVR CPU memory] tag=", BmvrTimingName(m_tag),
        " bytes=", m_bytes, " src=", m_src, " dst=", m_dst,
        " durationMs=", double(ns) / 1000000.0));
#endif
    } catch (...) { }
  }
};
}
