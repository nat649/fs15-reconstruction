#pragma once
// Linux ABI transport of the EXISTING Windows local_platform.cpp contract.
// Used only by the separately pinned, already prepared local-platform image.
// No Steam client, login, ownership, statistics, DLC or network is implemented.
#include <cstdint>
#include <cstring>
#include <iostream>
namespace ge6_local {
#define GE6_LOCAL_MS __attribute__((ms_abi))
inline void record(const char* text){std::cout<<"GE6_LOCAL_SERVICE "<<text<<'\n';}
inline GE6_LOCAL_MS uintptr_t zero(){return 0;}
inline GE6_LOCAL_MS uintptr_t app_id(){record("UTIL_APP_ID");return 313160;}
using Method=uintptr_t(GE6_LOCAL_MS*)();
struct Methods {Method values[24];};
inline Methods methods=[](){Methods out{};for(auto& fn:out.values)fn=zero;out.values[9]=app_id;return out;}();
struct Object {Methods* table;};
inline Object utils{&methods};
inline GE6_LOCAL_MS void* get_utils(){record("UTILS_MINIMAL");return &utils;}
inline GE6_LOCAL_MS bool initialize(){record("INIT_UNAVAILABLE");return false;}
inline GE6_LOCAL_MS bool restart(unsigned){record("NO_CLIENT_RESTART");return false;}
inline GE6_LOCAL_MS void* client(){record("CLIENT_UNAVAILABLE");return nullptr;}
inline GE6_LOCAL_MS void* user(){record("USER_UNAVAILABLE");return nullptr;}
inline GE6_LOCAL_MS void* apps(){record("APPS_UNAVAILABLE");return nullptr;}
inline GE6_LOCAL_MS void* stats(){record("USER_STATS_UNAVAILABLE");return nullptr;}
inline GE6_LOCAL_MS void shutdown(){record("SHUTDOWN_NOOP");}
inline GE6_LOCAL_MS void callbacks(){record("CALLBACKS_NOOP");}
inline GE6_LOCAL_MS void reg(void*,int){record("REGISTER_NOOP");}
inline GE6_LOCAL_MS void unreg(void*){record("UNREGISTER_NOOP");}
inline GE6_LOCAL_MS void dump(unsigned,unsigned,void*){record("MINIDUMP_NOOP");}
inline GE6_LOCAL_MS void comment(const char*){record("COMMENT_NOOP");}
inline uintptr_t resolve(const char* name){
#define LOCAL_BIND(n,f) if(!std::strcmp(name,n))return reinterpret_cast<uintptr_t>(&f)
    LOCAL_BIND("SteamUtils",get_utils);LOCAL_BIND("SteamAPI_Init",initialize);
    LOCAL_BIND("SteamAPI_RestartAppIfNecessary",restart);LOCAL_BIND("SteamClient",client);
    LOCAL_BIND("SteamUser",user);LOCAL_BIND("SteamApps",apps);LOCAL_BIND("SteamUserStats",stats);
    LOCAL_BIND("SteamAPI_Shutdown",shutdown);LOCAL_BIND("SteamAPI_RunCallbacks",callbacks);
    LOCAL_BIND("SteamAPI_RegisterCallback",reg);LOCAL_BIND("SteamAPI_UnregisterCallback",unreg);
    LOCAL_BIND("SteamAPI_WriteMiniDump",dump);LOCAL_BIND("SteamAPI_SetMiniDumpComment",comment);
#undef LOCAL_BIND
    return 0;
}
#undef GE6_LOCAL_MS
}
