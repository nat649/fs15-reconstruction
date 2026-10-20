#pragma once
// Authored Microsoft-x64 ABI adapters to upstream Linux OpenAL Soft.
// Device/context/source state and mixing belong to that library, not this host.
#include <AL/al.h>
#include <AL/alc.h>
#include <cstdint>
#include <cstring>
#include <string>
namespace ge6_audio {
#define GE6_AUDIO_MS __attribute__((ms_abi))
inline void(*trace)(const char*)=nullptr;
static_assert(sizeof(ALboolean)==1 && sizeof(ALCboolean)==1 && sizeof(ALint)==4 && sizeof(ALfloat)==4);
inline GE6_AUDIO_MS ALCdevice* alcOpenDevice(const ALCchar* name){
    trace("alcOpenDevice");
    // Windows OpenAL router implementation names have no Linux device identity.
    // Translate only its two legacy default selectors; preserve explicit names.
    if(name&&(!std::strcmp(name,"Generic Software")||!std::strcmp(name,"Generic Hardware"))){
        trace("OpenAL legacy default selector mapped to Linux default device");name=nullptr;
    }
    return ::alcOpenDevice(name);
}
#define AUDIO_WRAP(result,name,args,values) inline GE6_AUDIO_MS result name args{trace(#name);return ::name values;}
AUDIO_WRAP(void,alcGetIntegerv,(ALCdevice* d,ALCenum p,ALCsizei n,ALCint* v),(d,p,n,v))
AUDIO_WRAP(ALenum,alGetError,(),())
AUDIO_WRAP(void,alDeleteSources,(ALsizei n,const ALuint* v),(n,v))
AUDIO_WRAP(ALCcontext*,alcCreateContext,(ALCdevice* d,const ALCint* a),(d,a))
AUDIO_WRAP(const ALCchar*,alcGetString,(ALCdevice* d,ALCenum p),(d,p))
AUDIO_WRAP(void,alDistanceModel,(ALenum v),(v))
AUDIO_WRAP(void,alDopplerFactor,(ALfloat v),(v))
AUDIO_WRAP(void,alListenerfv,(ALenum p,const ALfloat* v),(p,v))
AUDIO_WRAP(void,alListenerf,(ALenum p,ALfloat v),(p,v))
AUDIO_WRAP(void,alGetListenerf,(ALenum p,ALfloat* v),(p,v))
AUDIO_WRAP(ALCdevice*,alcGetContextsDevice,(ALCcontext* c),(c))
AUDIO_WRAP(ALCcontext*,alcGetCurrentContext,(),())
AUDIO_WRAP(ALCenum,alcGetError,(ALCdevice* d),(d))
AUDIO_WRAP(void,alGenBuffers,(ALsizei n,ALuint* v),(n,v))
AUDIO_WRAP(ALboolean,alIsBuffer,(ALuint v),(v))
AUDIO_WRAP(void,alBufferData,(ALuint b,ALenum f,const ALvoid* d,ALsizei n,ALsizei rate),(b,f,d,n,rate))
AUDIO_WRAP(void,alDeleteBuffers,(ALsizei n,const ALuint* v),(n,v))
AUDIO_WRAP(void,alSourcei,(ALuint s,ALenum p,ALint v),(s,p,v))
AUDIO_WRAP(void,alSourcef,(ALuint s,ALenum p,ALfloat v),(s,p,v))
AUDIO_WRAP(void,alSourcefv,(ALuint s,ALenum p,const ALfloat* v),(s,p,v))
AUDIO_WRAP(void,alGetSourcei,(ALuint s,ALenum p,ALint* v),(s,p,v))
AUDIO_WRAP(void,alSourcePlay,(ALuint s),(s))
AUDIO_WRAP(void,alSourceUnqueueBuffers,(ALuint s,ALsizei n,ALuint* v),(s,n,v))
AUDIO_WRAP(void,alSourceQueueBuffers,(ALuint s,ALsizei n,const ALuint* v),(s,n,v))
AUDIO_WRAP(void,alSourcePause,(ALuint s),(s))
AUDIO_WRAP(void,alSourceStop,(ALuint s),(s))
AUDIO_WRAP(ALCboolean,alcMakeContextCurrent,(ALCcontext* c),(c))
AUDIO_WRAP(void,alcDestroyContext,(ALCcontext* c),(c))
AUDIO_WRAP(ALCboolean,alcCloseDevice,(ALCdevice* d),(d))
AUDIO_WRAP(void,alGenSources,(ALsizei n,ALuint* v),(n,v))
AUDIO_WRAP(const ALchar*,alGetString,(ALenum p),(p))
#undef AUDIO_WRAP
inline uintptr_t resolve(const std::string& name){
#define AUDIO_BIND(n) if(name==#n)return reinterpret_cast<uintptr_t>(&n)
    AUDIO_BIND(alcOpenDevice);AUDIO_BIND(alcGetIntegerv);AUDIO_BIND(alGetError);AUDIO_BIND(alDeleteSources);
    AUDIO_BIND(alcCreateContext);AUDIO_BIND(alcGetString);AUDIO_BIND(alDistanceModel);AUDIO_BIND(alDopplerFactor);
    AUDIO_BIND(alListenerfv);AUDIO_BIND(alListenerf);AUDIO_BIND(alGetListenerf);AUDIO_BIND(alcGetContextsDevice);
    AUDIO_BIND(alcGetCurrentContext);AUDIO_BIND(alcGetError);AUDIO_BIND(alGenBuffers);AUDIO_BIND(alIsBuffer);
    AUDIO_BIND(alBufferData);AUDIO_BIND(alDeleteBuffers);AUDIO_BIND(alSourcei);AUDIO_BIND(alSourcef);
    AUDIO_BIND(alSourcefv);AUDIO_BIND(alGetSourcei);AUDIO_BIND(alSourcePlay);AUDIO_BIND(alSourceUnqueueBuffers);
    AUDIO_BIND(alSourceQueueBuffers);AUDIO_BIND(alSourcePause);AUDIO_BIND(alSourceStop);AUDIO_BIND(alcMakeContextCurrent);
    AUDIO_BIND(alcDestroyContext);AUDIO_BIND(alcCloseDevice);AUDIO_BIND(alGenSources);AUDIO_BIND(alGetString);
#undef AUDIO_BIND
    return 0; // Never expose a System-V extension pointer to original Windows code.
}
#undef GE6_AUDIO_MS
}
