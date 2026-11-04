#include <Windows.h>
#include "game.h"
#include "bmvr_flags.h"
#include "bmvr_version.h"
#include <cstdint>
#include <cstring>

static LONG CALLBACK FaultVeh(PEXCEPTION_POINTERS info)
{
    if (!info || !info->ExceptionRecord || !info->ContextRecord)
        return EXCEPTION_CONTINUE_SEARCH;
    const DWORD code = info->ExceptionRecord->ExceptionCode;
    if (code != 0xC0000005 && code != 0xC000001D && code != 0xC0000094)
        return EXCEPTION_CONTINUE_SEARCH;
    static DWORD seen[8];
    static volatile LONG count = 0;
    const DWORD eip = info->ContextRecord->Eip;
    const LONG n = InterlockedIncrement(&count);
    if (n > 8)
        return EXCEPTION_CONTINUE_SEARCH;
    seen[n - 1] = eip;
    for (LONG i = 0; i < n - 1; ++i)
    {
        if (seen[i] == eip)
            return EXCEPTION_CONTINUE_SEARCH;
    }

    char body[1600];
    int used = snprintf(body, sizeof(body), "fault eip=%08X code=%08X\n", eip, code);
    HMODULE mod = nullptr;
    if (GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(static_cast<uintptr_t>(eip)), &mod) && mod)
    {
        char modPath[MAX_PATH]{};
        GetModuleFileNameA(mod, modPath, MAX_PATH);
        const char* base = strrchr(modPath, '\\');
        base = base ? base + 1 : modPath;
        used += snprintf(body + used, sizeof(body) - static_cast<size_t>(used),
            "  at %s+%08X\n", base, eip - reinterpret_cast<DWORD>(mod));
    }
    DWORD ebp = info->ContextRecord->Ebp;
    for (int frame = 0; frame < 12 && used < static_cast<int>(sizeof(body)) - 80; ++frame)
    {
        DWORD next = 0, ret = 0;
        __try
        {
            next = *reinterpret_cast<DWORD*>(static_cast<uintptr_t>(ebp));
            ret = *reinterpret_cast<DWORD*>(static_cast<uintptr_t>(ebp + 4));
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { break; }
        HMODULE sm = nullptr;
        if (ret && GetModuleHandleExA(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCSTR>(static_cast<uintptr_t>(ret)), &sm) && sm)
        {
            char modPath[MAX_PATH]{};
            GetModuleFileNameA(sm, modPath, MAX_PATH);
            const char* base = strrchr(modPath, '\\');
            base = base ? base + 1 : modPath;
            used += snprintf(body + used, sizeof(body) - static_cast<size_t>(used),
                "  #%d %s+%08X\n", frame, base, ret - reinterpret_cast<DWORD>(sm));
        }
        if (next <= ebp)
            break;
        ebp = next;
    }
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    wchar_t* slash = wcsrchr(path, L'\\');
    if (!slash)
        return EXCEPTION_CONTINUE_SEARCH;
    wcscpy_s(slash + 1, MAX_PATH - static_cast<size_t>(slash + 1 - path), L"bmvr_fault.txt");
    HANDLE h = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE)
    {
        DWORD wrote = 0;
        WriteFile(h, body, static_cast<DWORD>(used), &wrote, nullptr);
        CloseHandle(h);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

#include "bmvr_abi.h"

extern "C" void __cdecl L4D2VR_ShutdownSystemMouseInputSuppression();
extern "C" void __cdecl L4D2VR_ShutdownReShadeVRBridge();

static BOOL CALLBACK RestoreMonitorMode(HMONITOR monitor, HDC, LPRECT, LPARAM)
{
    MONITORINFOEXW info{};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(monitor, reinterpret_cast<MONITORINFO*>(&info)))
        return TRUE;
    DEVMODEW mode{};
    mode.dmSize = sizeof(mode);
    if (!EnumDisplaySettingsW(info.szDevice, ENUM_REGISTRY_SETTINGS, &mode))
        return TRUE;
    ChangeDisplaySettingsExW(info.szDevice, &mode, nullptr, 0, nullptr);
    return TRUE;
}

static void RestoreDesktopDisplayModes()
{
    EnumDisplayMonitors(nullptr, nullptr, RestoreMonitorMode, 0);
}

// Counts access violations (caught or not) so a guarded read that faults every
// frame shows up in bmvr_log; under Wine each one costs a kernel round trip.
namespace
{
    volatile LONG g_FaultCount = 0;
    struct FaultSlot { volatile LONG addr; volatile LONG hits; };
    FaultSlot g_FaultSlots[8]{};

    LONG CALLBACK CountFaults(PEXCEPTION_POINTERS info)
    {
        if (info && info->ExceptionRecord
            && info->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION)
        {
            InterlockedIncrement(&g_FaultCount);
            const LONG addr = static_cast<LONG>(reinterpret_cast<uintptr_t>(
                info->ExceptionRecord->ExceptionAddress));
            for (FaultSlot& s : g_FaultSlots)
            {
                if (s.addr == addr || InterlockedCompareExchange(&s.addr, addr, 0) == 0)
                {
                    InterlockedIncrement(&s.hits);
                    break;
                }
            }
        }
        return EXCEPTION_CONTINUE_SEARCH;
    }
}

namespace bmvr
{
    void LogFaultStats()
    {
        const LONG n = InterlockedExchange(&g_FaultCount, 0);
        LONG topAddr = 0, topHits = 0;
        for (FaultSlot& s : g_FaultSlots)
        {
            const LONG hits = InterlockedExchange(&s.hits, 0);
            const LONG addr = InterlockedExchange(&s.addr, 0);
            if (hits > topHits)
            {
                topHits = hits;
                topAddr = addr;
            }
        }
        if (n == 0)
            return;
        HMODULE mod = nullptr;
        char name[MAX_PATH] = "?";
        uintptr_t rva = static_cast<uintptr_t>(topAddr);
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCSTR>(static_cast<uintptr_t>(topAddr)), &mod) && mod)
        {
            GetModuleFileNameA(mod, name, MAX_PATH);
            rva -= reinterpret_cast<uintptr_t>(mod);
        }
        const char* base = std::strrchr(name, '\\');
        Log("Access violations since last report: %ld, top %ld at %s+0x%X",
            n, topHits, base ? base + 1 : name, static_cast<unsigned>(rva));
    }
}

static DWORD WINAPI InitBMVR(LPVOID param)
{
#ifdef _DEBUG
    AllocConsole();
    FILE* fp = nullptr;
    freopen_s(&fp, "CONOUT$", "w", stdout);
#endif
    // Three install copies (game root, bin\, DXVK folder) can map as distinct
    // modules. A second Game()/pose waiter calling WaitGetPoses wedged the
    // load-to-menu (2026-08-18, two "BMVR d3d9.dll loaded" then no LevelInit).
    HANDLE initGate = CreateMutexW(nullptr, TRUE, L"Local\\BlackMesaVR-InitBMVR");
    if (!initGate)
    {
        bmvr::Log("InitBMVR CreateMutex failed err=%lu", GetLastError());
        return 0;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        char path[MAX_PATH]{};
        GetModuleFileNameA(static_cast<HMODULE>(param), path, MAX_PATH);
        bmvr::Log("InitBMVR skipped duplicate d3d9.dll %s", path[0] ? path : "?");
        ReleaseMutex(initGate);
        CloseHandle(initGate);
        return 0;
    }

    // First log line: lets a shared bmvr_log.txt name the exact build.
    bmvr::Log("Black Mesa VR %s (commit %s %s, built %s %s)",
        BMVR_RELEASE, BMVR_COMMIT, BMVR_COMMIT_DATE, __DATE__, __TIME__);
    bmvr::InitFromDisk();
    char path[MAX_PATH]{};
    GetModuleFileNameA(static_cast<HMODULE>(param), path, MAX_PATH);
    bmvr::Log("BMVR d3d9.dll loaded (L4D2VR architecture, Black Mesa offsets) from %s",
        path[0] ? path : "?");
    HMODULE client = nullptr;
    for (int i=0; i<1800 && !client; ++i) {
        client=GetModuleHandleA("client.dll");
        if (!client) Sleep(100);
    }
    if (!client) { bmvr::Log("Game ABI: client.dll unavailable; VR hooks disabled"); return 0; }
    const auto* base=reinterpret_cast<const unsigned char*>(client);
    const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS*>(base+dos->e_lfanew);
    const unsigned char beta[] = {0x55,0x8B,0xEC,0x83,0xEC,0x24,0x53,0x56,0x57,0x8B,0x7D,0x08,0x8B,0xF1,0x85,0xFF};
    const unsigned char stable[] = {0x55,0x8B,0xEC,0x83,0xEC,0x24,0x53,0x56,0x8B,0x75,0x08,0x57,0x8B,0xF9,0x85,0xF6};
    if (nt->OptionalHeader.SizeOfImage > 0x29CC70+sizeof(beta) && std::memcmp(base+0x29CC70,beta,sizeof(beta))==0)
        bmvr::abi::build=bmvr::abi::Build::Beta26234270;
    else if (nt->OptionalHeader.SizeOfImage > 0x29BA10+sizeof(beta) && std::memcmp(base+0x29BA10,beta,sizeof(beta))==0)
        bmvr::abi::build=bmvr::abi::Build::Beta25844216;
    else if (nt->OptionalHeader.SizeOfImage > 0x29D930+sizeof(stable) && std::memcmp(base+0x29D930,stable,sizeof(stable))==0)
        bmvr::abi::build=bmvr::abi::Build::Stable19042901;
    else { bmvr::Log("Game ABI: unsupported client; VR hooks disabled"); return 0; }
    bmvr::Log("Game ABI selected: %s; common v53 gameplay fixes",bmvr::abi::build == bmvr::abi::Build::Beta26234270 ? "public-beta26234270" : bmvr::abi::Beta()?"public-beta25844216":"stable19042901");
    g_Game = new Game();
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
    {
        DisableThreadLibraryCalls(hModule);
        // DXVK NormalizePresentParameters honors this. Exclusive fullscreen
        // after VR_Init caused a ChangeDisplaySettings Reset loop, 1 FPS,
        // crash, and a leftover lower desktop resolution (2026-08-16).
        SetEnvironmentVariableA("DXVK_FORCE_WINDOWED", "1");
        bmvr::SetDllModule(hModule);
        char faultLog[8]{};
        if (GetEnvironmentVariableA("BMVR_FAULT_DIAG",faultLog,sizeof(faultLog))==1 && faultLog[0]=='1')
            AddVectoredExceptionHandler(1, FaultVeh);
        bmvr::StartWatchdog();
        bmvr::SetStage("dll_attach");
        CreateThread(nullptr, 0, InitBMVR, hModule, 0, nullptr);
        break;
    }
    case DLL_PROCESS_DETACH:
        RestoreDesktopDisplayModes();
        L4D2VR_ShutdownReShadeVRBridge();
        L4D2VR_ShutdownSystemMouseInputSuppression();
        break;
    default:
        break;
    }
    return TRUE;
}
