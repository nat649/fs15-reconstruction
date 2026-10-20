#pragma once
// Authored PCM and lifecycle checks; no original game audio is used.
#define AL_ALEXT_PROTOTYPES
#include <AL/alext.h>
#include <array>
#include <cmath>
#include <stdexcept>
namespace ge6_audio {
inline void check(){
    auto require=[](bool value,const char* reason){if(!value)throw std::runtime_error(reason);};
    require(!ge6_audio::alcOpenDevice("FS15-authored-nonexistent-device"),"Unknown OpenAL device was accepted");
    auto* physical=ge6_audio::alcOpenDevice("Generic Software");require(physical,"Cannot open selected Linux audio backend");
    require(ge6_audio::alcGetString(physical,ALC_DEVICE_SPECIFIER)!=nullptr,"Missing actual device name");
    ALCint major=0,minor=0;ge6_audio::alcGetIntegerv(physical,ALC_MAJOR_VERSION,1,&major);ge6_audio::alcGetIntegerv(physical,ALC_MINOR_VERSION,1,&minor);
    require(major==1&&minor>=1&&ge6_audio::alcCloseDevice(physical),"Device version/close differs");
    // Loopback is a real software mixer, independent of an audio daemon/device.
    auto* device=::alcLoopbackOpenDeviceSOFT(nullptr);require(device,"Cannot open OpenAL Soft loopback");
    const ALCint attributes[]{ALC_FORMAT_CHANNELS_SOFT,ALC_STEREO_SOFT,ALC_FORMAT_TYPE_SOFT,ALC_FLOAT_SOFT,ALC_FREQUENCY,48000,0};
    auto* context=ge6_audio::alcCreateContext(device,attributes);
    require(context&&ge6_audio::alcMakeContextCurrent(context)&&ge6_audio::alcGetCurrentContext()==context&&ge6_audio::alcGetContextsDevice(context)==device,"OpenAL context lifecycle differs");
    ge6_audio::alListenerf(AL_GAIN,0.5f);ALfloat gain=0;ge6_audio::alGetListenerf(AL_GAIN,&gain);require(gain==0.5f,"Listener float ABI differs");
    const ALfloat orientation[]{0,0,-1,0,1,0};ge6_audio::alListenerfv(AL_ORIENTATION,orientation);ge6_audio::alDopplerFactor(1.0f);ge6_audio::alDistanceModel(AL_NONE);
    ALuint buffers[2]{},source=0;ge6_audio::alGenBuffers(2,buffers);ge6_audio::alGenSources(1,&source);require(ge6_audio::alIsBuffer(buffers[0]),"OpenAL buffer not created");
    std::array<int16_t,4096> pcm{};for(size_t i=0;i<pcm.size();++i)pcm[i]=int16_t(std::sin(double(i)*2*3.141592653589793*440/48000)*16000);
    for(auto buffer:buffers)ge6_audio::alBufferData(buffer,AL_FORMAT_MONO16,pcm.data(),ALsizei(pcm.size()*sizeof(int16_t)),48000);
    ge6_audio::alSourcei(source,AL_SOURCE_RELATIVE,AL_TRUE);ge6_audio::alSourcef(source,AL_GAIN,0.75f);const ALfloat position[]{0,0,-1};ge6_audio::alSourcefv(source,AL_POSITION,position);
    ge6_audio::alSourceQueueBuffers(source,2,buffers);ALint count=0;ge6_audio::alGetSourcei(source,AL_BUFFERS_QUEUED,&count);require(count==2,"PCM queue differs");
    ge6_audio::alSourcePlay(source);ge6_audio::alGetSourcei(source,AL_SOURCE_STATE,&count);require(count==AL_PLAYING,"Source did not play");
    std::array<float,2048> output{};::alcRenderSamplesSOFT(device,output.data(),ALCsizei(output.size()/2));
    double energy=0;for(auto sample:output){require(std::isfinite(sample),"Nonfinite mixed sample");energy+=double(sample)*sample;}
    require(energy>1.0&&energy<1024,"OpenAL loopback did not mix authored PCM");
    ge6_audio::alSourcePause(source);ge6_audio::alGetSourcei(source,AL_SOURCE_STATE,&count);require(count==AL_PAUSED,"Pause state differs");
    ge6_audio::alSourcePlay(source);ge6_audio::alSourceStop(source);ge6_audio::alGetSourcei(source,AL_BUFFERS_PROCESSED,&count);require(count==2,"Stopped queue not processed");
    ALuint unqueued[2]{};ge6_audio::alSourceUnqueueBuffers(source,2,unqueued);require(unqueued[0]==buffers[0]&&unqueued[1]==buffers[1],"Queue order differs");
    ge6_audio::alSourcef(source,AL_GAIN,-1.0f);require(ge6_audio::alGetError()==AL_INVALID_VALUE&&ge6_audio::alGetError()==AL_NO_ERROR,"Actual AL errors were not preserved/cleared");
    ge6_audio::alDeleteSources(1,&source);ge6_audio::alDeleteBuffers(2,buffers);require(!ge6_audio::alIsBuffer(buffers[0])&&ge6_audio::alGetError()==AL_NO_ERROR&&ge6_audio::alcGetError(device)==ALC_NO_ERROR,"Audio cleanup/error differs");
    require(ge6_audio::alcMakeContextCurrent(nullptr),"Cannot clear context");ge6_audio::alcDestroyContext(context);require(ge6_audio::alcCloseDevice(device),"Cannot close loopback device");
}
}
