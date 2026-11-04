#include "bmvr_stall_capture.h"
#include "bmvr_flags.h"
#include <atomic>
#include <cstdio>
#include <cwchar>
#include <cstring>
#include <mutex>

namespace bmvr_stall {
namespace {
    SharedStage* stage = nullptr;
    HANDLE mapping = nullptr;
    DWORD ownerTid = 0;
    wchar_t mappingName[96]{}, probePath[MAX_PATH]{}, outputPath[MAX_PATH]{};
    std::once_flag initialized;
    thread_local unsigned depth = 0;
    thread_local const char* phase = "RenderView/preparation";

    void SetPhase(const char* name) {
        if (!stage || GetCurrentThreadId()!=ownerTid || !depth) return;
        InterlockedIncrement(&stage->sequence);
        phase = name;
        strncpy_s(stage->phase, name, _TRUNCATE);
        stage->phaseTick = GetTickCount();
        InterlockedIncrement(&stage->sequence);
    }

    DWORD WINAPI Watcher(LPVOID) {
        // Wine startup cost was ~1.9s in v22, longer than the remaining stall.
        // Start once during loading; the resident observer detects later stalls
        // without a CreateProcess call on the stalled frame.
        wchar_t command[1200]{};
        _snwprintf_s(command,_TRUNCATE,L"\"%s\" --watch %lu %lu \"%s\" \"%s\"",
            probePath,GetCurrentProcessId(),ownerTid,mappingName,outputPath);
        STARTUPINFOW si{};si.cb=sizeof si;
        PROCESS_INFORMATION pi{};
        if (!CreateProcessW(probePath,command,nullptr,nullptr,FALSE,CREATE_NO_WINDOW,
            nullptr,nullptr,&si,&pi)) {
            bmvr::Log("[BMVR stall v23] resident observer unavailable err=%lu",GetLastError());
            return 0;
        }
        CloseHandle(pi.hThread);
        bmvr::Log("[BMVR stall v23] resident observer started pid=%lu",pi.dwProcessId);
        WaitForSingleObject(pi.hProcess,INFINITE);
        DWORD code=0;GetExitCodeProcess(pi.hProcess,&code);CloseHandle(pi.hProcess);
        bmvr::Log("[BMVR stall v23] resident observer completed exit=%lu",code);
        return 0;
    }

    void Initialize() {
        char enabled[2]{};
        if (GetEnvironmentVariableA("BMVR_STALL_CAPTURE",enabled,sizeof enabled)!=1 || enabled[0]!='1') return;
        wchar_t exe[MAX_PATH]{};
        if (!GetModuleFileNameW(nullptr,exe,MAX_PATH)) return;
        wchar_t* slash=wcsrchr(exe,L'\\'); if (!slash) return; *slash=0;
        _snwprintf_s(outputPath,_TRUNCATE,L"%s\\VR\\diagnostics",exe);
        wchar_t overridePath[MAX_PATH]{};
        DWORD n=GetEnvironmentVariableW(L"BMVR_STALL_PATH",overridePath,MAX_PATH);
        if(n && n<MAX_PATH) wcscpy_s(outputPath,overridePath);
        _snwprintf_s(probePath,_TRUNCATE,L"%s\\VR\\diagnostics\\BMVRStallProbe.exe",exe);
        n=GetEnvironmentVariableW(L"BMVR_STALL_PROBE",overridePath,MAX_PATH);
        if(n && n<MAX_PATH) wcscpy_s(probePath,overridePath);
        if (GetFileAttributesW(probePath)==INVALID_FILE_ATTRIBUTES) {
            bmvr::Log("[BMVR stall v23] probe executable missing; collector disabled");return;
        }
        CreateDirectoryW(outputPath,nullptr);
        _snwprintf_s(mappingName,_TRUNCATE,L"Local\\BMVR_Stall_v23_%lu",GetCurrentProcessId());
        mapping=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(SharedStage),mappingName);
        if (!mapping) return;
        stage=static_cast<SharedStage*>(MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(SharedStage)));
        if (!stage) { CloseHandle(mapping);mapping=nullptr;return; }
        *stage=SharedStage{};stage->pid=GetCurrentProcessId();stage->tid=ownerTid;
        // This background thread must not outlive the code of an unloaded DLL.
        // Pin only outside DllMain, at the first opt-in render scope.
        HMODULE pinned=nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCWSTR>(&Watcher),&pinned)) { stage=nullptr;return; }
        HANDLE watcher=CreateThread(nullptr,0,Watcher,nullptr,0,nullptr);
        if (!watcher) { stage=nullptr;return; }
        CloseHandle(watcher);
        bmvr::Log("[BMVR stall v23] armed pid=%lu renderTid=%lu threshold=700ms pauseBudget=3 cooldown=20s resident=1",
            GetCurrentProcessId(),ownerTid);
    }
}

RenderScope::RenderScope(bool enabled) {
    if (!enabled) return;
    // Only the registered render thread writes the seqlock. No cross-thread
    // engine calls and no clocks/logs on disabled launches.
    std::call_once(initialized,[] { ownerTid=GetCurrentThreadId(); Initialize(); });
    if (!stage || GetCurrentThreadId()!=ownerTid || depth) return;
    owned=true; depth=1; phase="RenderView/preparation";
    InterlockedIncrement(&stage->sequence);
    ++stage->frame;stage->startTick=stage->phaseTick=GetTickCount();
    stage->active=1;stage->eligible=0;
    strncpy_s(stage->phase,phase,_TRUNCATE);
    InterlockedIncrement(&stage->sequence);
}
RenderScope::~RenderScope() {
    if (!owned) return;
    InterlockedIncrement(&stage->sequence);stage->active=0;stage->eligible=0;
    InterlockedIncrement(&stage->sequence);depth=0;
}
void RenderScope::SetEligible(bool value) {
    if (!owned) return;
    InterlockedIncrement(&stage->sequence);stage->eligible=value?1:0;
    InterlockedIncrement(&stage->sequence);
}
PhaseScope::PhaseScope(const char* name) {
    if (!stage || GetCurrentThreadId()!=ownerTid || !depth || !name) return;
    owned=true;previous=phase;SetPhase(name);
}
PhaseScope::~PhaseScope() { if (owned) SetPhase(previous); }
}
