#pragma once
// Authored bounded CryptoAPI random-provider transport to actual OpenSSL DRBGs.
// No Windows key containers, ownership results or cryptographic keys are faked.
#include <openssl/crypto.h>
#include <openssl/rand.h>
#include <cstdint>
#include <cstdio>
#include <map>
#include <memory>
#include <mutex>
namespace ge6_crypto {
#define GE6_CRYPTO_MS __attribute__((ms_abi))
inline void(*trace)(const char*)=nullptr;
inline void(*set_error)(uint32_t)=nullptr;
inline std::mutex mutex;
inline std::map<uintptr_t,std::shared_ptr<OSSL_LIB_CTX>> contexts;
inline uintptr_t next=0x77000000;
inline GE6_CRYPTO_MS int acquire(uintptr_t* output,const char* container,const char* provider,uint32_t type,uint32_t flags){
    trace("CryptAcquireContextA");if(!output){set_error(87);return 0;}*output=0;if(container||provider||type!=24||flags!=0xf0000000u){set_error(50);return 0;}
    auto context=std::shared_ptr<OSSL_LIB_CTX>(OSSL_LIB_CTX_new(),OSSL_LIB_CTX_free);uint8_t probe=0;if(!context||RAND_bytes_ex(context.get(),&probe,1,0)!=1){set_error(0x80090020u);return 0;}
    std::lock_guard<std::mutex> lock(mutex);auto handle=next++;contexts.emplace(handle,std::move(context));*output=handle;std::fprintf(stderr,"GE6_CRYPTO_PROVIDER_CREATED handle=0x%lx caller=0x%lx\n",handle,reinterpret_cast<uintptr_t>(__builtin_return_address(0)));return 1;
}
inline GE6_CRYPTO_MS int random(uintptr_t handle,uint32_t count,uint8_t* bytes){
    trace("CryptGenRandom");std::shared_ptr<OSSL_LIB_CTX> context;{std::lock_guard<std::mutex> lock(mutex);auto found=contexts.find(handle);if(found==contexts.end()){set_error(0x80090001u);return 0;}context=found->second;}
    if(count&&!bytes){set_error(87);return 0;}if(count&&RAND_bytes_ex(context.get(),bytes,count,0)!=1){set_error(0x80090020u);return 0;}return 1;
}
inline GE6_CRYPTO_MS int release(uintptr_t handle,uint32_t flags){trace("CryptReleaseContext");if(flags){set_error(87);return 0;}std::lock_guard<std::mutex> lock(mutex);if(!contexts.erase(handle)){set_error(0x80090001u);return 0;}return 1;}
#undef GE6_CRYPTO_MS
}
