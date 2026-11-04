#pragma once

#include <array>
#include <atomic>

#include "util_env.h"
#include "log/log.h"

namespace dxvk {

  // The front-end normally removes redundant geometry binds, but restore and
  // per-draw upload paths can also rebind unchanged slots. Keep the dirty flags
  // set independently by buffer invalidation and command-list/render-pass setup.
  // Graphics-written buffers stay on the original path: checkGraphicsHazards
  // and updateVertex/IndexBufferBinding use VI dirty flags for read barriers.
  // No counters or per-call logging are active in the normal launch.
  inline bool BmvrSkipIdenticalGeometryBind(uint32_t kind, bool identical, bool hazardFree = true) {
    struct Options {
      bool disabled;
      bool diagnostics;
    };
    static const Options options = {
      env::getEnvVar("BMVR_CPU_BATCH_DISABLE") == "1",
      env::getEnvVar("BMVR_BIND_DIAG") == "1"
    };

    const bool skip = identical && hazardFree && !options.disabled;
    if (options.diagnostics) {
      struct Counts {
        std::atomic<uint64_t> calls{0};
        std::atomic<uint64_t> identical{0};
        std::atomic<uint64_t> skipped{0};
      };
      static std::array<Counts, 4> counts;
      auto& current = counts[kind];
      if (identical)
        current.identical.fetch_add(1, std::memory_order_relaxed);
      if (skip)
        current.skipped.fetch_add(1, std::memory_order_relaxed);
      const uint64_t calls = current.calls.fetch_add(1, std::memory_order_relaxed) + 1;
      if ((calls & 65535u) == 0) {
        Logger::info(str::format("[BMVR geometry binds] kind=", kind,
          " calls=", calls,
          " identical=", current.identical.load(std::memory_order_relaxed),
          " skipped=", current.skipped.load(std::memory_order_relaxed)));
      }
    }
    return skip;
  }

}
