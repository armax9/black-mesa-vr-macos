#include <cstdio>
#include <cstring>
#include <cstddef>
#include <cmath>
#include "src/openxr_bridge_protocol.h"
#include "src/openxr_cpu_gpu_bridge.h"
#include "src/bmvr_abi.h"
#include "experiments/openxr-fsr/fsr_plan.h"
#define CHECK(x) do { if(!(x)){ std::printf("FAIL: %s at %d\n",#x,__LINE__); return 1; } }while(0)
__declspec(noinline) int GuardedRead(volatile const int* p) {
 __try { volatile int local=0; return *p+local; }
 __except(EXCEPTION_EXECUTE_HANDLER) { return -7; }
}
int main(){
#if defined(_M_IX86)
 CHECK(GuardedRead(nullptr)==-7);
#endif
 CHECK(sizeof(bmvr_shared_gpu::Slot)==128);
 CHECK(offsetof(bmvr_shared_gpu::Slot,eyeTangents)==88);
 CHECK(sizeof(bmvr_shared_gpu::Control)==448);
 CHECK(L4D2VR_OPENXR_BRIDGE_VERSION==20);
 L4D2VROpenXrBridgeState state{};state.reservedTail0=bmvr_shared_gpu::HelperCapability;
 state.loadingState=1;CHECK(state.reservedTail0==bmvr_shared_gpu::HelperCapability);
 state.loadingState=0;CHECK(state.reservedTail0==bmvr_shared_gpu::HelperCapability);
 CHECK(bmvr_shared_gpu::Version==2);
 CHECK(sizeof(bmvr_fsr::EasuConstants)==96 && sizeof(bmvr_fsr::RcasConstants)==48);
 bmvr::abi::build=bmvr::abi::Build::Stable19042901; CHECK(!bmvr::abi::Beta() && !bmvr::abi::Latest());
 bmvr::abi::build=bmvr::abi::Build::Beta25844216; CHECK(bmvr::abi::Beta() && !bmvr::abi::Latest());
 bmvr::abi::build=bmvr::abi::Build::Beta26234270; CHECK(bmvr::abi::Beta() && bmvr::abi::Latest());
 uint32_t con[4][4]{}; FsrEasuCon(con[0],con[1],con[2],con[3],1776.f,1920.f,1776.f,1920.f,2368.f,2560.f);
 float scale=0,offset=0;std::memcpy(&scale,&con[0][0],4);std::memcpy(&offset,&con[0][2],4);
 CHECK(std::fabs(scale-.75f)<1e-6f && std::fabs(offset+.125f)<1e-6f);
 bmvr_shared_gpu::Slot slot{};slot.eyeTangents[0][0]=-1.1f;slot.eyeTangents[1][0]=-.8f;
 bmvr_shared_gpu::Slot copy{};std::memcpy(&copy,&slot,sizeof(slot));
 CHECK(copy.eyeTangents[0][0]==-1.1f && copy.eyeTangents[1][0]==-.8f);
 std::printf("PASS bits=%zu bridge=%zu slot=%zu control=%zu EASU=%.2f offset=%.3f\n",sizeof(void*)*8,sizeof(L4D2VROpenXrBridgeState),sizeof(slot),sizeof(bmvr_shared_gpu::Control),scale,offset);
 return 0;
}
