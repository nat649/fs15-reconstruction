#pragma once
// Authored Microsoft-x64 ABI transport to POSIX IPv4 UDP sockets.
// Version negotiation does not establish complete Winsock/provider parity.
#include <arpa/inet.h>
#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
namespace ge6_net {
#define GE6_NET_MS __attribute__((ms_abi))
inline void(*trace)(const char*)=nullptr;
inline std::recursive_mutex registry_mutex;
inline uint32_t references=0;
inline thread_local int last_error=0;
inline uintptr_t next_socket=0x60000000;
inline std::map<uintptr_t,int> sockets;
struct StartupData {
    uint16_t version,highest,max_sockets,max_datagram;
    uint64_t vendor;
    char description[257],status[129];
};
static_assert(sizeof(StartupData)==408 && offsetof(StartupData,description)==16 && offsetof(StartupData,status)==273);
inline int translate(int e){switch(e){case EACCES:case EPERM:return 10013;case EFAULT:return 10014;case EINVAL:return 10022;case EMFILE:case ENFILE:return 10024;case EAGAIN:return 10035;case EINPROGRESS:return 10036;case EBADF:case ENOTSOCK:return 10038;case EMSGSIZE:return 10040;case EPROTONOSUPPORT:return 10043;case EAFNOSUPPORT:return 10047;case EADDRINUSE:return 10048;case EADDRNOTAVAIL:return 10049;case ENETDOWN:return 10050;case ENETUNREACH:return 10051;case ECONNRESET:return 10054;case ENOBUFS:case ENOMEM:return 10055;case ETIMEDOUT:return 10060;case ECONNREFUSED:return 10061;case EHOSTUNREACH:return 10065;default:return 10022;}}
inline int fail(int e){last_error=e;return -1;}
inline int descriptor(uintptr_t socket){std::lock_guard<std::recursive_mutex> guard(registry_mutex);if(!references){fail(10093);return -1;}auto found=sockets.find(socket);if(found==sockets.end()){fail(10038);return -1;}return found->second;}
inline GE6_NET_MS int startup(uint16_t requested,StartupData* out){std::lock_guard<std::recursive_mutex> guard(registry_mutex);
    trace("WSAStartup");if(!out)return 10014;
    auto major=requested&255,minor=requested>>8;if(major<1)return 10092;
    if(references==0xffffffffu)return 10067;
    int probe=::socket(AF_INET,SOCK_DGRAM|SOCK_CLOEXEC,0);if(probe<0)return 10091;::close(probe);
    std::memset(out,0,sizeof(*out));out->version=major==1?uint16_t(minor?0x101:1):uint16_t(2|(std::min(major>2?2:minor,2)<<8));out->highest=0x202;
    std::strcpy(out->description,"GE6 Linux POSIX socket transport (partial Winsock)");std::strcpy(out->status,"IPv4 UDP available");++references;return 0;
}
inline GE6_NET_MS int cleanup(){std::lock_guard<std::recursive_mutex> guard(registry_mutex);trace("WSACleanup");if(!references)return fail(10093);if(--references==0){for(auto item:sockets)::close(item.second);sockets.clear();}return 0;}
inline GE6_NET_MS int get_error(){return last_error;}
inline GE6_NET_MS uintptr_t create(int family,int type,int protocol){std::lock_guard<std::recursive_mutex> guard(registry_mutex);trace("socket");if(!references){fail(10093);return ~uintptr_t(0);}if(family!=2){fail(10047);return ~uintptr_t(0);}if(type!=2||(protocol!=0&&protocol!=17)){fail(10043);return ~uintptr_t(0);}int opened=::socket(AF_INET,SOCK_DGRAM|SOCK_CLOEXEC,protocol);if(opened<0){fail(translate(errno));return ~uintptr_t(0);}auto handle=next_socket++;sockets.emplace(handle,opened);return handle;}
inline bool address(const void* raw,int size,sockaddr_in& result){if(!raw||size<16){fail(10014);return false;}std::memcpy(&result,raw,16);if(result.sin_family!=2){fail(10047);return false;}return true;}
inline GE6_NET_MS int close(uintptr_t socket){std::lock_guard<std::recursive_mutex> guard(registry_mutex);trace("closesocket");auto fd=descriptor(socket);if(fd<0)return -1;int result=::close(fd);sockets.erase(socket);return result<0?fail(translate(errno)):0;}
inline GE6_NET_MS int bind(uintptr_t socket,const void* raw,int size){trace("bind");auto fd=descriptor(socket);if(fd<0)return -1;sockaddr_in target{};if(!address(raw,size,target))return -1;return ::bind(fd,reinterpret_cast<sockaddr*>(&target),sizeof(target))<0?fail(translate(errno)):0;}
inline GE6_NET_MS int name(uintptr_t socket,void* raw,int* size){trace("getsockname");auto fd=descriptor(socket);if(fd<0)return -1;if(!raw||!size||*size<16)return fail(10014);sockaddr_in target{};socklen_t capacity=sizeof(target);if(::getsockname(fd,reinterpret_cast<sockaddr*>(&target),&capacity)<0)return fail(translate(errno));std::memcpy(raw,&target,16);*size=16;return 0;}
inline GE6_NET_MS int send_to(uintptr_t socket,const void* data,int count,int flags,const void* raw,int size){trace("sendto");auto fd=descriptor(socket);if(fd<0)return -1;if(count<0||flags!=0)return fail(10022);sockaddr_in target{};if(!address(raw,size,target))return -1;auto result=::sendto(fd,data,size_t(count),MSG_NOSIGNAL,reinterpret_cast<sockaddr*>(&target),16);return result<0?fail(translate(errno)):int(result);}
inline GE6_NET_MS int receive_from(uintptr_t socket,void* data,int count,int flags,void* raw,int* size){trace("recvfrom");auto fd=descriptor(socket);if(fd<0)return -1;if(count<0||(flags!=0&&flags!=2))return fail(10022);if(raw&&(!size||*size<16))return fail(10014);sockaddr_in target{};iovec buffer{data,size_t(count)};msghdr message{};message.msg_name=raw?&target:nullptr;message.msg_namelen=raw?16:0;message.msg_iov=&buffer;message.msg_iovlen=1;auto result=::recvmsg(fd,&message,flags==2?MSG_PEEK:0);if(result<0)return fail(translate(errno));if(raw){std::memcpy(raw,&target,16);*size=16;}if(message.msg_flags&MSG_TRUNC)return fail(10040);return int(result);}
inline uintptr_t resolve(const std::string& symbol){
#define NET_BIND(n,o,f) if(symbol==n||symbol==o)return reinterpret_cast<uintptr_t>(&f)
    NET_BIND("WSAStartup","ordinal:115",startup);NET_BIND("WSACleanup","ordinal:116",cleanup);NET_BIND("WSAGetLastError","ordinal:111",get_error);
    NET_BIND("socket","ordinal:23",create);NET_BIND("closesocket","ordinal:3",close);NET_BIND("bind","ordinal:2",bind);NET_BIND("getsockname","ordinal:6",name);NET_BIND("sendto","ordinal:20",send_to);NET_BIND("recvfrom","ordinal:17",receive_from);
#undef NET_BIND
    return 0;
}
#undef GE6_NET_MS
}
