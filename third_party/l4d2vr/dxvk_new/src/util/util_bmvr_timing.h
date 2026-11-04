#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
namespace dxvk {
enum BmvrCpuMemoryTag : unsigned {
  BmvrCpuConstantCopy = 20,
  BmvrCpuStagingClear = 21,
  BmvrCpuTexturePack = 22,
  BmvrCpuEyeReadbackCopy = 23
};
constexpr unsigned BmvrTimingCount = 24;
inline const char* BmvrTimingName(unsigned i) {
  static const char* const names[BmvrTimingCount] = {"createInstance", "getOptimizedPipeline",
    "addStage", "SynchronizeCsThread", "WaitForResource", "ThrottleAllocationFence",
    "LockImage", "LockBuffer", "DrawPrimitiveUP", "QueryEventPending", "QueryOcclusionPending", "QueryOtherPending",
    "QueryEventGetData", "QueryOcclusionGetData", "QueryOtherGetData", "DeviceExclusiveLock",
    "CreateShaderModule", "CreateImageResource", "PipelineCacheSave", "DeviceMutexWait",
    "CpuConstantCopy", "CpuStagingClear", "CpuTexturePack", "CpuEyeReadbackCopy"};
  return names[i];
}
struct BmvrTimingSnapshot {
  std::array<uint64_t,BmvrTimingCount> ns{},calls{};
};
struct BmvrTimingCounters {
  std::array<std::atomic<uint64_t>,BmvrTimingCount> ns{},calls{};
};
inline BmvrTimingCounters& BmvrTimings() { static BmvrTimingCounters counters; return counters; }
inline unsigned BmvrTimingTag(const char* name) {
  for (unsigned i=0;i<BmvrTimingCount;i++) if (!std::strcmp(name,BmvrTimingName(i))) return i;
  return BmvrTimingCount;
}
inline void BmvrTimingAdd(unsigned tag,uint64_t ns) {
  if (tag>=BmvrTimingCount) return;
  auto& c=BmvrTimings();
  c.ns[tag].fetch_add(ns,std::memory_order_relaxed);
  c.calls[tag].fetch_add(1,std::memory_order_relaxed);
}
inline BmvrTimingSnapshot BmvrTimingRead() {
  BmvrTimingSnapshot s;auto& c=BmvrTimings();
  for (unsigned i=0;i<BmvrTimingCount;i++) {
    s.ns[i]=c.ns[i].load(std::memory_order_relaxed);
    s.calls[i]=c.calls[i].load(std::memory_order_relaxed);
  }
  return s;
}
}
