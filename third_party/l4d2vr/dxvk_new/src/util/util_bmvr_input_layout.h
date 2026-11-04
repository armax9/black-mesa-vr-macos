#pragma once
#include <atomic>
#include "util_env.h"
#include "log/log.h"
#include "util_string.h"
namespace dxvk {
template<class Signature> inline bool BmvrSameInputSemantics(const Signature& a,const Signature& b) {
  if(a.elemCount!=b.elemCount) return false;
  for(uint32_t i=0;i<a.elemCount;i++) if(!(a.elems[i].semantic==b.elems[i].semantic)) return false;
  return true;
}
inline bool BmvrInputLayoutOptEnabled() {static const bool v=env::getEnvVar("BMVR_CPU_BATCH_DISABLE")!="1";return v;}
inline void BmvrCountInputLayouts(bool reused,bool alreadyDirty) {
  static const bool enabled=env::getEnvVar("BMVR_INPUT_DIAG")=="1";
  if(!enabled) return;
  static std::atomic<uint64_t> calls{0},equivalent{0},avoided{0};
  if(reused)equivalent.fetch_add(1,std::memory_order_relaxed);
  if(reused&&!alreadyDirty)avoided.fetch_add(1,std::memory_order_relaxed);
  auto n=calls.fetch_add(1,std::memory_order_relaxed)+1;
  if((n&1023u)==0) Logger::info(str::format("[BMVR input v43] transitions=",n," equivalent=",equivalent.load(std::memory_order_relaxed)," avoidedInvalidations=",avoided.load(std::memory_order_relaxed)));
}
}
