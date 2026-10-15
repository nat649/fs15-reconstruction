// Authored local-only platform adapter. No client, login, ownership or network
// service is implemented. Apps/User/UserStats deliberately return null.
#include <windows.h>
#include <cstdint>

static volatile LONG seen=0;
static void record(LONG bit,const char* name,DWORD bytes) {
    if(InterlockedOr(&seen,bit)&bit)return;
    wchar_t path[32768];
    const DWORD size=GetEnvironmentVariableW(L"FS15_LOCAL_TRACE",path,32768);
    if(!size||size>=32768)return;
    HANDLE file=CreateFileW(path,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return;
    DWORD written;WriteFile(file,name,bytes,&written,nullptr);CloseHandle(file);
}
#define TRACE(BIT,NAME) record(1L<<(BIT),NAME "\n",sizeof(NAME))
static std::uintptr_t zero(){return 0;}
static std::uintptr_t app_id(){TRACE(13,"UTIL_APP_ID");return 313160;}
struct LocalUtils {std::uintptr_t (*methods[24])();};
static LocalUtils methods{};
struct Object {LocalUtils* methods;};
static Object utils{&methods};
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID) {
    if(reason==DLL_PROCESS_ATTACH){DisableThreadLibraryCalls(module);for(auto& fn:methods.methods)fn=zero;methods.methods[9]=app_id;}
    return TRUE;
}
#define EXPORT extern "C" __declspec(dllexport)
EXPORT void* SteamUtils(){TRACE(0,"UTILS");return &utils;}
EXPORT void SteamAPI_RegisterCallback(void*,int){TRACE(1,"REGISTER_NOOP");}
EXPORT void* SteamUserStats(){TRACE(2,"USER_STATS_UNAVAILABLE");return nullptr;}
EXPORT void SteamAPI_WriteMiniDump(unsigned,unsigned,void*){TRACE(3,"MINIDUMP_NOOP");}
EXPORT void SteamAPI_SetMiniDumpComment(const char*){TRACE(4,"COMMENT_NOOP");}
EXPORT void SteamAPI_Shutdown(){TRACE(5,"SHUTDOWN_NOOP");}
EXPORT void* SteamClient(){TRACE(6,"CLIENT_UNAVAILABLE");return nullptr;}
EXPORT void SteamAPI_UnregisterCallback(void*){TRACE(7,"UNREGISTER_NOOP");}
EXPORT bool SteamAPI_Init(){TRACE(8,"INIT_UNAVAILABLE");return false;}
EXPORT void* SteamApps(){TRACE(9,"APPS_UNAVAILABLE");return nullptr;}
EXPORT bool SteamAPI_RestartAppIfNecessary(unsigned){TRACE(10,"NO_CLIENT_RESTART");return false;}
EXPORT void SteamAPI_RunCallbacks(){TRACE(11,"CALLBACKS_NOOP");}
EXPORT void* SteamUser(){TRACE(12,"USER_UNAVAILABLE");return nullptr;}
EXPORT unsigned fs15_local_adapter_version(){return 1;}
