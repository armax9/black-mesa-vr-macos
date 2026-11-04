#include "bmvr_stall_capture.h"
#include <TlHelp32.h>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <cstring>
#include <algorithm>

static_assert(sizeof(void*)==4,"The observer must be Win32, like Black Mesa");
namespace {
    struct Raw {
        CONTEXT context{};
        BYTE stack[8192]{};
        DWORD stackBase=0, stackBytes=0;
        DWORD contextError=0, stackError=0, queryError=0;
        DWORD previousSuspend=DWORD(-1), suspendError=0;
        DWORD resumeResult=DWORD(-1), resumeError=0;
        DWORD pauseMs=0, contextMs=0, copyMs=0;
        DWORD triggerTick=0, captureTick=0, captureFrameAgeMs=0, phaseAgeMs=0;
        bmvr_stall::StageSnapshot pausedStage{};
        bool pausedStageOk=false;
        bool contextOk=false, stackOk=false, phaseSame=false;
    };
    struct GuardState {
        HANDLE target=nullptr;
        // 0=not started, 1=Suspend in flight, 2=owned increment,
        // 3=resume in flight, 4=resume completed, 5=Suspend failed.
        std::atomic<unsigned> state{0};
        std::atomic<DWORD> pauseTick{0}, resumeTick{0}, resumeResult{DWORD(-1)}, resumeError{0};
        std::atomic<bool> ready{false}, mainDone{false}, forced{false};
        const wchar_t* output=nullptr;
    };
    bool ResumeOnce(GuardState& g,bool forced) {
        unsigned expected=2;
        if (!g.state.compare_exchange_strong(expected,3)) return false;
        if (forced) g.forced.store(true);
        const DWORD result=ResumeThread(g.target);
        const DWORD error=result==DWORD(-1)?GetLastError():0;
        g.resumeResult.store(result);g.resumeError.store(error);g.resumeTick.store(GetTickCount());
        g.state.store(4);
        return true;
    }
    DWORD WINAPI Guard(LPVOID parameter) {
        auto& g=*static_cast<GuardState*>(parameter);
        g.ready.store(true);
        bool timedOut=false;
        for (;;) {
            if (g.mainDone.load()) return 0;
            const unsigned state=g.state.load();
            const DWORD tick=g.pauseTick.load();
            if (state==2 && GetTickCount()-tick>=50) {
                ResumeOnce(g,true);timedOut=true;
            }
            // No event waits / fd-cache operations in this guard. Wine's
            // NtResumeThread and NtYieldExecution use independent thread RPC
            // state / native yield, unlike context-reading waits.
            if (timedOut && g.state.load()==4 && GetTickCount()-tick>=1000) {
                // The target has been resumed (or the error is explicitly
                // recorded). A stuck reader cannot keep the observer alive.
                HANDLE file=CreateFileW(g.output,GENERIC_WRITE,FILE_SHARE_READ,nullptr,
                    CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
                if(file!=INVALID_HANDLE_VALUE) {
                    char message[300]{};DWORD written=0;
                    const int n=snprintf(message,sizeof message,
                        "BMVR v23 observer timeout: context reader did not return.\n"
                        "forcedResume=1 resumeResult=%lu resumeError=%lu\n"
                        "No context or stack is valid.\nEND_BMVR_SNAPSHOT\n",g.resumeResult.load(),g.resumeError.load());
                    WriteFile(file,message,DWORD(n),&written,nullptr);CloseHandle(file);
                }
                TerminateProcess(GetCurrentProcess(),12);
            }
            SwitchToThread();
        }
    }

    void Capture(HANDLE process,HANDLE thread,const bmvr_stall::SharedStage* shared,
        const bmvr_stall::StageSnapshot& expected,Raw& raw,GuardState& guard) {
        guard.pauseTick.store(GetTickCount());guard.state.store(1);
        raw.previousSuspend=SuspendThread(thread);
        if(raw.previousSuspend==DWORD(-1)) {
            raw.suspendError=GetLastError();guard.state.store(5);return;
        }
        guard.pauseTick.store(GetTickCount());guard.state.store(2);
        raw.captureTick=guard.pauseTick.load();
        // Even a pre-suspended thread gained OUR increment. Undo exactly that
        // one increment, preserving other debuggers' suspension ownership.
        if(raw.previousSuspend!=0) {
            ResumeOnce(guard,false);
            while(guard.state.load()==3) SwitchToThread();
            raw.resumeResult=guard.resumeResult.load();raw.resumeError=guard.resumeError.load();
            raw.pauseMs=guard.resumeTick.load()-guard.pauseTick.load();
            return;
        }
        // The request phase may have advanced before the thread was stopped.
        // Take authoritative metadata from the shared mapping while paused;
        // preserve useful captures within the SAME long render frame.
        raw.pausedStageOk=bmvr_stall::ReadStage(shared,raw.pausedStage)
            && raw.pausedStage.pid==expected.pid && raw.pausedStage.tid==expected.tid
            && raw.pausedStage.frame==expected.frame && raw.pausedStage.active
            && raw.pausedStage.eligible;
        if(!raw.pausedStageOk) {
            ResumeOnce(guard,false);
            while(guard.state.load()==3) SwitchToThread();
            raw.resumeResult=guard.resumeResult.load();raw.resumeError=guard.resumeError.load();
            raw.pauseMs=guard.resumeTick.load()-guard.pauseTick.load();
            return;
        }
        raw.captureFrameAgeMs=raw.captureTick-raw.pausedStage.startTick;
        raw.phaseAgeMs=raw.captureTick-raw.pausedStage.phaseTick;
        raw.context.ContextFlags=CONTEXT_CONTROL|CONTEXT_INTEGER;
        const DWORD t0=GetTickCount();
#ifdef BMVR_STALL_TESTING
        char delay[16]{};
        GetEnvironmentVariableA("BMVR_STALL_TEST_DELAY_MS",delay,sizeof delay);
        if(unsigned wait=static_cast<unsigned>(strtoul(delay,nullptr,10))) Sleep(wait);
#endif
        raw.contextOk=GetThreadContext(thread,&raw.context)!=FALSE;
        if(!raw.contextOk) raw.contextError=GetLastError();
        raw.contextMs=GetTickCount()-t0;
        if(raw.contextOk && !guard.forced.load()) {
            MEMORY_BASIC_INFORMATION region{};
            const DWORD address=raw.context.Esp;
            // Do not touch PAGE_GUARD or cross a committed region: Wine's RPM
            // for local memory may otherwise consume the target's guard page.
            if(VirtualQueryEx(process,reinterpret_cast<void*>(uintptr_t(address)),&region,sizeof region)
                && region.State==MEM_COMMIT && !(region.Protect&(PAGE_GUARD|PAGE_NOACCESS))
                && (region.Protect&(PAGE_READONLY|PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY))
                && uintptr_t(region.BaseAddress)+region.RegionSize>uintptr_t(address)) {
                const uintptr_t end=uintptr_t(region.BaseAddress)+region.RegionSize;
                const SIZE_T bytes=std::min<SIZE_T>(sizeof raw.stack,end-uintptr_t(address));
                SIZE_T copied=0;raw.stackBase=address;
                const DWORD copyStart=GetTickCount();
                raw.stackOk=ReadProcessMemory(process,reinterpret_cast<void*>(uintptr_t(address)),
                    raw.stack,bytes,&copied)!=FALSE;
                if(!raw.stackOk) raw.stackError=GetLastError();
                raw.copyMs=GetTickCount()-copyStart;
                raw.stackBytes=static_cast<DWORD>(copied);
            } else raw.queryError=ERROR_NOACCESS;
        }
        bmvr_stall::StageSnapshot after{};
        raw.phaseSame=bmvr_stall::ReadStage(shared,after) && after.active && after.eligible
            && after.pid==expected.pid
            && after.frame==expected.frame && after.tid==expected.tid
            && after.phaseTick==raw.pausedStage.phaseTick
            && !strcmp(after.phase,raw.pausedStage.phase)
            && !guard.forced.load();
        ResumeOnce(guard,false);
        // Guard may own the resume. Never close its handle or write a valid
        // snapshot before its actual ResumeThread call has returned.
        while(guard.state.load()==3) SwitchToThread();
        raw.resumeResult=guard.resumeResult.load();raw.resumeError=guard.resumeError.load();
        raw.pauseMs=guard.resumeTick.load()-guard.pauseTick.load();
    }

    struct Module { DWORD base=0,size=0; wchar_t name[256]{}; };
    struct Modules { Module row[192];unsigned count=0; };
    void LoadModules(DWORD pid,Modules& modules) {
        HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE,pid);
        if(snapshot==INVALID_HANDLE_VALUE) return;
        MODULEENTRY32W entry{};entry.dwSize=sizeof entry;
        if(Module32FirstW(snapshot,&entry)) do {
            if(modules.count==192) break;
            auto& m=modules.row[modules.count++];m.base=DWORD(uintptr_t(entry.modBaseAddr));m.size=entry.modBaseSize;
            wcsncpy_s(m.name,entry.szModule,_TRUNCATE);
        } while(Module32NextW(snapshot,&entry));
        CloseHandle(snapshot);
    }
    const Module* FindModule(const Modules& modules,DWORD address) {
        for(unsigned i=0;i<modules.count;++i)
            if(address>=modules.row[i].base && address-modules.row[i].base<modules.row[i].size) return &modules.row[i];
        return nullptr;
    }
    void Address(FILE* file,const Modules& modules,DWORD address) {
        if(const auto* m=FindModule(modules,address))
            fprintf(file,"0x%08lX %ls+0x%lX",address,m->name,address-m->base);
        else fprintf(file,"0x%08lX (outside listed guest modules)",address);
    }
    bool StackWord(const Raw& raw,DWORD address,DWORD& value) {
        if(address<raw.stackBase || address-raw.stackBase>raw.stackBytes
            || raw.stackBytes-(address-raw.stackBase)<4) return false;
        memcpy(&value,raw.stack+(address-raw.stackBase),4);return true;
    }
    int Report(const wchar_t* path,const bmvr_stall::StageSnapshot& s,const Raw& raw,bool forced) {
        FILE* file=nullptr;_wfopen_s(&file,path,L"wb");if(!file) return 13;
        const bool valid=raw.contextOk && raw.phaseSame && !forced
            && raw.previousSuspend==0 && raw.resumeResult==1;
        fprintf(file,"BMVR v23 guest stall snapshot\npid=%lu tid=%lu frame=%lu phase=%s reportFrameAgeMs=%lu\n",
            s.pid,s.tid,s.frame,s.phase,GetTickCount()-s.startTick);
        fprintf(file,"valid=%d previousSuspend=%lu suspendError=%lu resumeResult=%lu resumeError=%lu forcedResume=%d\n",
            valid?1:0,raw.previousSuspend,raw.suspendError,raw.resumeResult,raw.resumeError,forced?1:0);
        fprintf(file,"pauseMs=%lu contextMs=%lu copyMs=%lu phaseSame=%d contextError=%lu stackError=%lu regionError=%lu stackBytes=%lu\n",
            raw.pauseMs,raw.contextMs,raw.copyMs,raw.phaseSame?1:0,raw.contextError,raw.stackError,raw.queryError,raw.stackBytes);
        fprintf(file,"triggerTick=%lu captureTick=%lu triggerToCaptureMs=%lu captureFrameAgeMs=%lu capturePhaseAgeMs=%lu\n",
            raw.triggerTick,raw.captureTick,raw.captureTick?raw.captureTick-raw.triggerTick:0,raw.captureFrameAgeMs,raw.phaseAgeMs);
        fprintf(file,"Guest x86 state; a native Wine/Metal thunk can expose only its guest boundary.\n"
            "Frame-pointer and stack-word candidates below are not a guaranteed symbolic unwind.\n");
        if(!valid) {fputs("END_BMVR_SNAPSHOT\n",file);fclose(file);return 14;}
        Modules modules{};LoadModules(s.pid,modules); // AFTER resume
        fprintf(file,"EIP=");Address(file,modules,raw.context.Eip);fprintf(file,"\nESP=0x%08lX EBP=0x%08lX\n",raw.context.Esp,raw.context.Ebp);
        fprintf(file,"EAX=%08lX EBX=%08lX ECX=%08lX EDX=%08lX ESI=%08lX EDI=%08lX EFLAGS=%08lX\n",
            raw.context.Eax,raw.context.Ebx,raw.context.Ecx,raw.context.Edx,raw.context.Esi,raw.context.Edi,raw.context.EFlags);
        fprintf(file,"\nFrame-pointer chain candidates:\n");
        DWORD fp=raw.context.Ebp;
        for(unsigned i=0;i<64;++i) {
            DWORD next=0,ret=0;
            if((fp&3)||!StackWord(raw,fp,next)||!StackWord(raw,fp+4,ret)) break;
            fprintf(file,"%u fp=%08lX return=",i,fp);Address(file,modules,ret);fputc('\n',file);
            if(next<=fp || next-raw.stackBase>=raw.stackBytes) break;fp=next;
        }
        fprintf(file,"\nExecutable stack-word candidates (may include data):\n");
        HANDLE process=OpenProcess(PROCESS_QUERY_INFORMATION,FALSE,s.pid);
        unsigned shown=0;
        for(DWORD off=0;off+4<=raw.stackBytes && shown<128;off+=4) {
            DWORD word=0;memcpy(&word,raw.stack+off,4);
            if(!FindModule(modules,word)) continue;
            MEMORY_BASIC_INFORMATION region{};
            if(!process || !VirtualQueryEx(process,reinterpret_cast<void*>(uintptr_t(word)),&region,sizeof region)
                || !(region.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY))) continue;
            fprintf(file,"stack+%04lX=",off);Address(file,modules,word);fputc('\n',file);++shown;
        }
        if(process) CloseHandle(process);
        fprintf(file,"\nGuest modules:\n");
        for(unsigned i=0;i<modules.count;++i) fprintf(file,"%08lX size=%08lX %ls\n",modules.row[i].base,modules.row[i].size,modules.row[i].name);
        fprintf(file,"\nRaw bounded stack words:\n");
        for(DWORD off=0;off+4<=raw.stackBytes;off+=4) {
            DWORD word=0;memcpy(&word,raw.stack+off,4);
            if(!(off%32)) fprintf(file,"\n%08lX:",raw.stackBase+off);
            fprintf(file," %08lX",word);
        }
        fputs("\nEND_BMVR_SNAPSHOT\n",file);fclose(file);return 0;
    }
}

static int Failure(const wchar_t* path,int code,const char* message,DWORD error=0) {
    FILE* file=nullptr;_wfopen_s(&file,path,L"wb");
    if(file) {fprintf(file,"BMVR v23 observer: %s\nvalid=0 exit=%d error=%lu\nEND_BMVR_SNAPSHOT\n",message,code,error);fclose(file);}
    return code;
}
namespace {
    struct ObserverHandles {
        HANDLE mapping=nullptr,process=nullptr,thread=nullptr;
        const bmvr_stall::SharedStage* shared=nullptr;
        ~ObserverHandles() {
            if(thread) CloseHandle(thread);
            if(process) CloseHandle(process);
            if(shared) UnmapViewOfFile(shared);
            if(mapping) CloseHandle(mapping);
        }
        ObserverHandles()=default;
        ObserverHandles(const ObserverHandles&)=delete;
        ObserverHandles& operator=(const ObserverHandles&)=delete;
    };
    int OpenObserver(ObserverHandles& h,DWORD pid,DWORD tid,const wchar_t* name,DWORD& error) {
        h.mapping=OpenFileMappingW(FILE_MAP_READ,FALSE,name);
        if(!h.mapping) {error=GetLastError();return 3;}
        h.shared=static_cast<const bmvr_stall::SharedStage*>(MapViewOfFile(h.mapping,FILE_MAP_READ,0,0,sizeof(bmvr_stall::SharedStage)));
        if(!h.shared) {error=GetLastError();return 3;}
        if(h.shared->magic!=bmvr_stall::Magic || h.shared->pid!=pid || h.shared->tid!=tid) {
            error=ERROR_INVALID_DATA;return 3;
        }
        h.process=OpenProcess(PROCESS_VM_READ|PROCESS_QUERY_INFORMATION|SYNCHRONIZE,FALSE,pid);
        if(!h.process) {error=GetLastError();return 5;}
        h.thread=OpenThread(THREAD_SUSPEND_RESUME|THREAD_GET_CONTEXT|THREAD_QUERY_INFORMATION,FALSE,tid);
        if(!h.thread) {error=GetLastError();return 5;}
        if(GetProcessIdOfThread(h.thread)!=pid) {error=ERROR_INVALID_DATA;return 5;}
        return 0;
    }
    // Guard construction and the freshness recheck occur BEFORE suspension.
    // A stale candidate never calls SuspendThread and never consumes the pause
    // budget or the successful-event cooldown.
    int Sample(ObserverHandles& h,const bmvr_stall::StageSnapshot& candidate,
        const wchar_t* path,DWORD triggerTick,Raw& raw,bool& forced,DWORD& error) {
        GuardState guard;guard.target=h.thread;guard.output=path;
        HANDLE guardian=CreateThread(nullptr,0,Guard,&guard,0,nullptr);
        if(!guardian) {error=GetLastError();return 6;}
        while(!guard.ready.load()) SwitchToThread();
#ifdef BMVR_STALL_TESTING
        char delay[16]{};
        GetEnvironmentVariableA("BMVR_STALL_TEST_PRE_CAPTURE_DELAY_MS",delay,sizeof delay);
        if(unsigned wait=static_cast<unsigned>(strtoul(delay,nullptr,10))) Sleep(wait);
#endif
        bmvr_stall::StageSnapshot fresh{};
        const bool eligible=bmvr_stall::ReadStage(h.shared,fresh)
            && fresh.pid==candidate.pid && fresh.tid==candidate.tid
            && fresh.frame==candidate.frame && fresh.active && fresh.eligible;
        raw.triggerTick=triggerTick;
        if(eligible) Capture(h.process,h.thread,h.shared,fresh,raw,guard);
        forced=guard.forced.load();guard.mainDone.store(true);
        WaitForSingleObject(guardian,INFINITE);CloseHandle(guardian);
        return eligible?0:4;
    }
    int WatchStatus(const wchar_t* path,DWORD pid,DWORD tid,const char* message,
        unsigned pauses,unsigned stale,int exitCode=0,DWORD error=0) {
        FILE* file=nullptr;_wfopen_s(&file,path,L"wb");if(!file) return 13;
        fprintf(file,"BMVR v23 resident observer\nready=1 pid=%lu tid=%lu\n"
            "state=%s pauses=%u/3 stale=%u/12 precheck-skipped=%u exit=%d error=%lu\nEND_BMVR_WATCH\n",
            pid,tid,message,pauses,stale,stale,exitCode,error);
        fclose(file);return exitCode;
    }
    int Watch(DWORD pid,DWORD tid,const wchar_t* name,const wchar_t* directory) {
        // All handle opens and CRT/runtime loading complete before publishing
        // ready=1. The game does not spawn another process for later stalls.
        wchar_t status[MAX_PATH]{};
        _snwprintf_s(status,_TRUNCATE,L"%s\\watch-%lu.txt",directory,pid);
        if(!CreateDirectoryW(directory,nullptr) && GetLastError()!=ERROR_ALREADY_EXISTS)
            return Failure(status,13,"diagnostic directory unavailable; no suspension",GetLastError());
        ObserverHandles h;DWORD error=0;
        const int opened=OpenObserver(h,pid,tid,name,error);
        if(opened) return Failure(status,opened,"resident target handles unavailable; no suspension",error);
        unsigned pauses=0,stale=0;
        DWORD lastFrame=0,lastPauseTick=0;
        if(WatchStatus(status,pid,tid,"waiting",pauses,stale)!=0) return 13;
        while(pauses<3) {
            const DWORD alive=WaitForSingleObject(h.process,100);
            if(alive==WAIT_OBJECT_0) return WatchStatus(status,pid,tid,"target-exited",pauses,stale);
            if(alive!=WAIT_TIMEOUT) return WatchStatus(status,pid,tid,"process-wait-failed",pauses,stale,5,GetLastError());
            bmvr_stall::StageSnapshot candidate{};
            if(!bmvr_stall::ReadStage(h.shared,candidate) || candidate.pid!=pid || candidate.tid!=tid
                || !candidate.active || !candidate.eligible) continue;
            const DWORD now=GetTickCount();
            if(now-candidate.startTick<700 || candidate.frame==lastFrame
                || (lastPauseTick && now-lastPauseTick<20000)) continue;
            lastFrame=candidate.frame;
            wchar_t path[MAX_PATH]{};
            _snwprintf_s(path,_TRUNCATE,L"%s\\stall-%lu-%u.txt",directory,pid,pauses+1);
            Raw raw{};bool forced=false;
            const int sampled=Sample(h,candidate,path,now,raw,forced,error);
            if(sampled==4) {
                ++stale;
                wchar_t stalePath[MAX_PATH]{};
                _snwprintf_s(stalePath,_TRUNCATE,L"%s\\stale-%lu-%u.txt",directory,pid,stale);
                Failure(stalePath,4,"render progressed before capture; no suspension");
                if(stale>=12) return WatchStatus(status,pid,tid,"stale-attempt-limit",pauses,stale,4);
                WatchStatus(status,pid,tid,"precheck-skipped",pauses,stale);
                continue;
            }
            if(sampled) return WatchStatus(status,pid,tid,"resume-guard-unavailable",pauses,stale,sampled,error);
            if(raw.previousSuspend==DWORD(-1)) {
                Failure(path,7,"SuspendThread failed; no owned pause",raw.suspendError);
                return WatchStatus(status,pid,tid,"suspend-failed",pauses,stale,7,raw.suspendError);
            }
            ++pauses;lastPauseTick=GetTickCount();
            const auto& captured=raw.pausedStageOk?raw.pausedStage:candidate;
            const int report=Report(path,captured,raw,forced);
            if(report==13) return WatchStatus(status,pid,tid,"snapshot-write-failed",pauses,stale,13);
            if(raw.resumeResult!=1) return WatchStatus(status,pid,tid,"resume-not-confirmed",pauses,stale,14,raw.resumeError);
            if(!raw.contextOk && raw.contextError) return WatchStatus(status,pid,tid,"context-failed",pauses,stale,14,raw.contextError);
            WatchStatus(status,pid,tid,report==0?"captured":"invalid-snapshot",pauses,stale);
        }
        return WatchStatus(status,pid,tid,"pause-budget-complete",pauses,stale);
    }
}
int wmain(int argc,wchar_t** argv) {
    if(argc==6 && !wcscmp(argv[1],L"--watch"))
        return Watch(wcstoul(argv[2],nullptr,10),wcstoul(argv[3],nullptr,10),argv[4],argv[5]);
    if(argc!=7 || wcscmp(argv[1],L"--capture")) return 2;
    const DWORD pid=wcstoul(argv[2],nullptr,10),tid=wcstoul(argv[3],nullptr,10),frame=wcstoul(argv[4],nullptr,10);
    ObserverHandles h;DWORD error=0;
    const int opened=OpenObserver(h,pid,tid,argv[5],error);
    if(opened) return Failure(argv[6],opened,"target handles unavailable; no suspension",error);
    bmvr_stall::StageSnapshot candidate{};
    if(!bmvr_stall::ReadStage(h.shared,candidate)||candidate.frame!=frame||!candidate.active||!candidate.eligible)
        return Failure(argv[6],4,"render progressed during observer startup; no suspension");
    Raw raw{};bool forced=false;
    const int sampled=Sample(h,candidate,argv[6],GetTickCount(),raw,forced,error);
    if(sampled) return Failure(argv[6],sampled,"render progressed before capture or guard unavailable; no suspension",error);
    return Report(argv[6],raw.pausedStageOk?raw.pausedStage:candidate,raw,forced);
}
