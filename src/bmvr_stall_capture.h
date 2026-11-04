#pragma once
#include <Windows.h>
#include <cstdint>

// Local, opt-in diagnostics. No camera/input math or new engine hooks.
namespace bmvr_stall {
constexpr DWORD Magic = 0x32534D42; // BMS2, Win32 writer and observer
struct SharedStage {
    DWORD magic = Magic;
    volatile LONG sequence = 0;
    DWORD pid = 0, tid = 0, frame = 0, startTick = 0, phaseTick = 0;
    DWORD active = 0, eligible = 0;
    char phase[64]{};
};
struct StageSnapshot {
    DWORD pid = 0, tid = 0, frame = 0, startTick = 0, phaseTick = 0;
    DWORD active = 0, eligible = 0;
    char phase[64]{};
};
inline bool ReadStage(const SharedStage* p, StageSnapshot& s) {
    if (!p || p->magic != Magic) return false;
    for (int attempt = 0; attempt < 3; ++attempt) {
        const LONG before = p->sequence;
        MemoryBarrier();
        if (before & 1) continue;
        s.pid=p->pid; s.tid=p->tid; s.frame=p->frame;
        s.startTick=p->startTick; s.phaseTick=p->phaseTick;
        s.active=p->active; s.eligible=p->eligible;
        for (unsigned i=0; i<sizeof s.phase; ++i) s.phase[i]=p->phase[i];
        MemoryBarrier();
        if (before == p->sequence) return true;
    }
    return false;
}
class RenderScope {
    bool owned = false;
public:
    explicit RenderScope(bool enabled = true);
    ~RenderScope();
    void SetEligible(bool value);
    RenderScope(const RenderScope&) = delete;
    RenderScope& operator=(const RenderScope&) = delete;
};
class PhaseScope {
    const char* previous = nullptr;
    bool owned = false;
public:
    explicit PhaseScope(const char* name);
    ~PhaseScope();
    PhaseScope(const PhaseScope&) = delete;
    PhaseScope& operator=(const PhaseScope&) = delete;
};
}
