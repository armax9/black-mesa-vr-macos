#pragma once
#include <array>
#include <d3d9.h>
#include <Windows.h>
#include "game.h"
// Diagnostics only: fixed query pool, one sample/60 frames, no GetData FLUSH.
namespace bmvr_gpu_probe {
struct Slot {
 std::array<IDirect3DQuery9*,3> stamp{};IDirect3DQuery9* freq=nullptr;IDirect3DQuery9* disjoint=nullptr;
 bool pending=false;DWORD frame=0,issued=0;LONGLONG cpuStart=0,cpuMid=0,cpuEnd=0;
};
struct Probe {
 std::array<Slot,8> slots{};int active=-1;unsigned serial=0,results=0,dropped=0;bool disabled=false;
 static bool enabled(){static const bool v=[] {char b[8]{};return GetEnvironmentVariableA("BMVR_GPU_DIAG",b,sizeof(b))==1 && b[0]=='1';}();return v;}
 static LONGLONG clock(){LARGE_INTEGER t{};QueryPerformanceCounter(&t);return t.QuadPart;}
 static double cpuMs(LONGLONG n){static const double scale=[] {LARGE_INTEGER f{};QueryPerformanceFrequency(&f);return f.QuadPart>0?1000.0/f.QuadPart:0.;}();return n*scale;}
 void release(Slot& s){for(auto*& q:s.stamp)if(q){q->Release();q=nullptr;}if(s.freq){s.freq->Release();s.freq=nullptr;}if(s.disjoint){s.disjoint->Release();s.disjoint=nullptr;}s.pending=false;}
 void poll(){
  for(auto& s:slots)if(s.pending){
   UINT64 t[3]{},f=0;BOOL disjoint=FALSE;
   HRESULT hr=s.stamp[2]->GetData(&t[2],sizeof(t[2]),0);
   if(hr==S_FALSE){if(GetTickCount()-s.issued>10000){Game::logMsg("[BMVR GPU v34] result timeout frame=%lu; stopping probe without GPU wait",s.frame);disabled=true;}continue;}
   if(hr!=S_OK){Game::logMsg("[BMVR GPU v34] GetData failed hr=0x%08X",unsigned(hr));disabled=true;continue;}
   if(s.stamp[0]->GetData(&t[0],sizeof(t[0]),0)!=S_OK || s.stamp[1]->GetData(&t[1],sizeof(t[1]),0)!=S_OK || s.freq->GetData(&f,sizeof(f),0)!=S_OK || s.disjoint->GetData(&disjoint,sizeof(disjoint),0)!=S_OK)continue;
   s.pending=false;
   if(disjoint || !f || t[2]<t[1] || t[1]<t[0]){Game::logMsg("[BMVR GPU v34] invalid timestamp sample frame=%lu disjoint=%d freq=%llu",s.frame,int(disjoint),static_cast<unsigned long long>(f));continue;}
   Game::logMsg("[BMVR GPU v34] frame=%lu cpuPairMs=%.3f cpuLeftMs=%.3f cpuRightMs=%.3f gpuSpanMs=%.3f gpuLeftSpanMs=%.3f gpuRightSpanMs=%.3f frequency=%llu readyAgeMs=%lu (GPU span can include idle gaps)",s.frame,cpuMs(s.cpuEnd-s.cpuStart),cpuMs(s.cpuMid-s.cpuStart),cpuMs(s.cpuEnd-s.cpuMid),1000.*double(t[2]-t[0])/f,1000.*double(t[1]-t[0])/f,1000.*double(t[2]-t[1])/f,static_cast<unsigned long long>(f),GetTickCount()-s.issued);
   if(++results>=150)disabled=true;
  }
 }
 void begin(IDirect3DDevice9* device,DWORD frame){
  active=-1;if(!enabled() || disabled || !device)return;poll();if(disabled || (++serial%60)!=1)return;
  for(unsigned i=0;i<slots.size();i++)if(!slots[i].pending){auto& s=slots[i];
   if(!s.freq){bool ok=device->CreateQuery(D3DQUERYTYPE_TIMESTAMPFREQ,&s.freq)==S_OK && device->CreateQuery(D3DQUERYTYPE_TIMESTAMPDISJOINT,&s.disjoint)==S_OK;
    for(auto*& q:s.stamp)if(ok)ok=device->CreateQuery(D3DQUERYTYPE_TIMESTAMP,&q)==S_OK;
    if(!ok){release(s);disabled=true;Game::logMsg("[BMVR GPU v34] timestamp queries unsupported; no rendering behavior changed");return;}}
   if(s.disjoint->Issue(D3DISSUE_BEGIN)!=S_OK || s.stamp[0]->Issue(D3DISSUE_END)!=S_OK){disabled=true;return;}
   s.frame=frame;s.issued=GetTickCount();s.cpuStart=clock();s.cpuMid=s.cpuStart;active=int(i);return;
  }++dropped;
 }
 void middle(){if(active>=0){auto& s=slots[active];s.cpuMid=clock();s.stamp[1]->Issue(D3DISSUE_END);}}
 void end(){if(active>=0){auto& s=slots[active];s.cpuEnd=clock();s.stamp[2]->Issue(D3DISSUE_END);s.freq->Issue(D3DISSUE_END);s.disjoint->Issue(D3DISSUE_END);s.pending=true;active=-1;}}
};
// Bounded process-lifetime diagnostic pool. Avoid COM teardown under loader lock.
inline Probe& instance(){static Probe* p=new Probe;return *p;}
}
