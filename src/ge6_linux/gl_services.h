#pragma once
// Authored WGL ABI to native GLX; rendering remains in the supplied GE6 image.
#include "window_services.h"
#ifdef GE6_X11
#include <atomic>
#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glx.h>
namespace ge6_gl {
#define GE6_GL_MS __attribute__((ms_abi))
using namespace ge6_display;
inline std::vector<GLXFBConfig> formats;
inline std::map<uintptr_t,int> pixel_formats;
struct Context {GLXContext native;uintptr_t dc;};
inline std::map<uintptr_t,Context> contexts;
inline thread_local uintptr_t current=0;
inline std::atomic<uint64_t> draw_calls{0},swapped_frames{0};
inline std::atomic<bool> captured_draw{false};
inline bool initialize(){if(!connect())return false;if(!formats.empty())return true;int count=0;auto* values=glXGetFBConfigs(display,DefaultScreen(display),&count);for(int i=0;i<count;++i){int id=0,drawable=0,render=0;glXGetFBConfigAttrib(display,values[i],GLX_VISUAL_ID,&id);glXGetFBConfigAttrib(display,values[i],GLX_DRAWABLE_TYPE,&drawable);glXGetFBConfigAttrib(display,values[i],GLX_RENDER_TYPE,&render);if(unsigned(id)==XVisualIDFromVisual(DefaultVisual(display,0))&&(drawable&GLX_WINDOW_BIT)&&(render&GLX_RGBA_BIT))formats.push_back(values[i]);}XFree(values);return !formats.empty();}
inline int attribute(int format,int name){int value=0;glXGetFBConfigAttrib(display,formats.at(size_t(format-1)),name,&value);return value;}
inline bool descriptor(int format,void* raw){if(!raw||format<1||size_t(format)>formats.size())return false;std::memset(raw,0,40);store<uint16_t>(raw,0,40);store<uint16_t>(raw,2,1);store<uint32_t>(raw,4,uint32_t(0x24|(attribute(format,GLX_DOUBLEBUFFER)?1:0)|(attribute(format,GLX_STEREO)?2:0)));auto* p=static_cast<uint8_t*>(raw);p[9]=uint8_t(attribute(format,GLX_RED_SIZE)+attribute(format,GLX_GREEN_SIZE)+attribute(format,GLX_BLUE_SIZE));p[10]=uint8_t(attribute(format,GLX_RED_SIZE));p[12]=uint8_t(attribute(format,GLX_GREEN_SIZE));p[14]=uint8_t(attribute(format,GLX_BLUE_SIZE));p[16]=uint8_t(attribute(format,GLX_ALPHA_SIZE));p[19]=uint8_t(attribute(format,GLX_ACCUM_RED_SIZE));p[20]=uint8_t(attribute(format,GLX_ACCUM_GREEN_SIZE));p[21]=uint8_t(attribute(format,GLX_ACCUM_BLUE_SIZE));p[22]=uint8_t(attribute(format,GLX_ACCUM_ALPHA_SIZE));p[18]=uint8_t(p[19]+p[20]+p[21]+p[22]);p[23]=uint8_t(attribute(format,GLX_DEPTH_SIZE));p[24]=uint8_t(attribute(format,GLX_STENCIL_SIZE));p[25]=uint8_t(attribute(format,GLX_AUX_BUFFERS));return true;}
inline GE6_GL_MS int describe(uintptr_t dc,int format,uint32_t size,void* raw){trace("DescribePixelFormat");std::lock_guard<std::recursive_mutex> lock(mutex);if(dcs.find(dc)==dcs.end()||!initialize()){set_error(6);return 0;}if(raw&&(size<40||!descriptor(format,raw))){set_error(87);return 0;}return int(formats.size());}
inline GE6_GL_MS int choose(uintptr_t dc,const void* raw){trace("ChoosePixelFormat");std::lock_guard<std::recursive_mutex> lock(mutex);if(!raw||field<uint16_t>(raw,0)!=40||dcs.find(dc)==dcs.end()||!initialize()){set_error(87);return 0;}auto* request=static_cast<const uint8_t*>(raw);auto flags=field<uint32_t>(raw,4);for(size_t i=0;i<formats.size();++i){uint8_t value[40]{};descriptor(int(i+1),value);if((field<uint32_t>(value,4)&flags)!=flags)continue;bool matches=value[8]==request[8];for(auto at:{9,10,12,14,16,18,19,20,21,22,23,24})matches=matches&&value[at]>=request[at];if(matches)return int(i+1);}set_error(2000);return 0;}
inline GE6_GL_MS int set_pixel(uintptr_t dc,int format,const void*){trace("SetPixelFormat");std::lock_guard<std::recursive_mutex> lock(mutex);if(dcs.find(dc)==dcs.end()||!initialize()||format<1||size_t(format)>formats.size()){set_error(2000);return 0;}auto previous=pixel_formats.find(dc);if(previous!=pixel_formats.end())return previous->second==format;pixel_formats[dc]=format;return 1;}
inline GE6_GL_MS int get_pixel(uintptr_t dc){trace("GetPixelFormat");std::lock_guard<std::recursive_mutex> lock(mutex);auto value=pixel_formats.find(dc);return value==pixel_formats.end()?0:value->second;}
inline GE6_GL_MS uintptr_t create(uintptr_t dc){trace("wglCreateContext");std::lock_guard<std::recursive_mutex> lock(mutex);auto selected=pixel_formats.find(dc);if(selected==pixel_formats.end()){set_error(2000);return 0;}auto* visual=glXGetVisualFromFBConfig(display,formats.at(size_t(selected->second-1)));if(!visual)return 0;auto context=glXCreateContext(display,visual,nullptr,True);XFree(visual);if(!context){set_error(2000);return 0;}auto handle=next++;contexts.emplace(handle,Context{context,dc});std::fprintf(stderr,"GE6_GLX_CONTEXT_CREATE handle=0x%lx dc=0x%lx caller=0x%lx\n",handle,dc,reinterpret_cast<uintptr_t>(__builtin_return_address(0)));return handle;}
inline GE6_GL_MS int make_current(uintptr_t dc,uintptr_t context){trace("wglMakeCurrent");std::lock_guard<std::recursive_mutex> lock(mutex);if(!context){auto result=glXMakeCurrent(display,0,nullptr);if(result)current=0;return result;}auto found=contexts.find(context);auto drawable=dcs.find(dc);if(found==contexts.end()||drawable==dcs.end()){set_error(6);return 0;}if(!glXMakeCurrent(display,drawable->second.drawable,found->second.native))return 0;current=context;std::fprintf(stderr,"GE6_GLX_CURRENT handle=0x%lx renderer=%s version=%s caller=0x%lx\n",context,glGetString(GL_RENDERER),glGetString(GL_VERSION),reinterpret_cast<uintptr_t>(__builtin_return_address(0)));return 1;}
inline GE6_GL_MS int remove(uintptr_t context){trace("wglDeleteContext");std::lock_guard<std::recursive_mutex> lock(mutex);auto found=contexts.find(context);if(found==contexts.end()||context==current){set_error(6);return 0;}glXDestroyContext(display,found->second.native);contexts.erase(found);return 1;}
inline GE6_GL_MS int share(uintptr_t,uintptr_t){trace("wglShareLists");set_error(50);return 0;} // GLX sharing must be selected at creation; no unsupported sharing success.
inline GE6_GL_MS const char* extensions(uintptr_t){trace("wglGetExtensionsStringARB");return "";} // No WGL extension adapter is advertised yet.
inline GE6_GL_MS int swap(uintptr_t dc){
    trace("SwapBuffers");std::lock_guard<std::recursive_mutex> lock(mutex);auto found=dcs.find(dc);if(found==dcs.end()||!current){set_error(6);return 0;}
    timespec before{},after{};clock_gettime(CLOCK_MONOTONIC,&before);glXSwapBuffers(display,found->second.drawable);clock_gettime(CLOCK_MONOTONIC,&after);
    auto frame=++swapped_frames;auto draws=draw_calls.load();auto elapsed=(int64_t(after.tv_sec)-int64_t(before.tv_sec))*1000000+(int64_t(after.tv_nsec)-int64_t(before.tv_nsec))/1000;
    if(frame<=10||frame%30==0)std::fprintf(stderr,"GE6_NATIVE_SWAP frame=%lu draws=%lu dc=0x%lx context=0x%lx caller=0x%lx duration_us=%ld\n",frame,draws,dc,current,reinterpret_cast<uintptr_t>(__builtin_return_address(0)),long(elapsed));
    if(frame==1||(draws&&!captured_draw.exchange(true))){ge6_windows::capture_on_stop();}
    return 1;
}
inline uintptr_t resolve(const std::string&,const std::string&);
inline GE6_GL_MS uintptr_t procedure(const char* name){trace("wglGetProcAddress");auto address=name?resolve("OPENGL32.dll",name):0;if(!address&&name)std::fprintf(stderr,"GE6_GL_PROC_UNAVAILABLE name=%s caller=0x%lx\n",name,reinterpret_cast<uintptr_t>(__builtin_return_address(0)));return address;}
template<auto Function>struct Bridge;
template<class R,class... A,R(*Function)(A...)>struct Bridge<Function>{inline static std::atomic<const char*> name{"unnamed"};inline static std::atomic<uint64_t> calls{0};static __attribute__((noinline)) GE6_GL_MS R call(A... args){auto count=++calls;auto address=reinterpret_cast<uintptr_t>(Function);if(address==reinterpret_cast<uintptr_t>(&glDrawArrays)||address==reinterpret_cast<uintptr_t>(&glDrawElements)||address==reinterpret_cast<uintptr_t>(&glDrawArraysInstanced)||address==reinterpret_cast<uintptr_t>(&glDrawElementsInstanced))++draw_calls;if(count==1)std::fprintf(stderr,"GE6_NATIVE_GL_CALL name=%s caller=0x%lx context=0x%lx\n",name.load(std::memory_order_relaxed),reinterpret_cast<uintptr_t>(__builtin_return_address(0)),current);return Function(args...);}};
inline uintptr_t resolve(const std::string& library,const std::string& name){
#define GL_BIND(n,f) if(name==n)return reinterpret_cast<uintptr_t>(&f)
    if(library=="GDI32.dll"){GL_BIND("ChoosePixelFormat",choose);GL_BIND("DescribePixelFormat",describe);GL_BIND("SetPixelFormat",set_pixel);GL_BIND("GetPixelFormat",get_pixel);GL_BIND("SwapBuffers",swap);}
    if(library=="OPENGL32.dll"){
        GL_BIND("wglCreateContext",create);GL_BIND("wglMakeCurrent",make_current);GL_BIND("wglDeleteContext",remove);GL_BIND("wglShareLists",share);GL_BIND("wglGetProcAddress",procedure);GL_BIND("wglGetExtensionsStringARB",extensions);
#define CORE_GL(n) if(name==#n){Bridge<&n>::name.store(#n,std::memory_order_relaxed);return reinterpret_cast<uintptr_t>(&Bridge<&n>::call);}
        CORE_GL(glGetString);CORE_GL(glGetIntegerv);CORE_GL(glGetError);CORE_GL(glClear);CORE_GL(glClearColor);CORE_GL(glViewport);CORE_GL(glFlush);CORE_GL(glFinish);CORE_GL(glReadPixels);
        CORE_GL(glTexParameterf);CORE_GL(glDeleteTextures);CORE_GL(glPixelStorei);CORE_GL(glCullFace);CORE_GL(glCopyTexSubImage2D);CORE_GL(glReadBuffer);CORE_GL(glGetTexImage);CORE_GL(glTexSubImage2D);CORE_GL(glTexParameterfv);CORE_GL(glTexImage2D);CORE_GL(glTexImage1D);CORE_GL(glTexParameteri);CORE_GL(glBindTexture);CORE_GL(glGenTextures);CORE_GL(glGetFloatv);CORE_GL(glFrontFace);CORE_GL(glHint);CORE_GL(glLineWidth);CORE_GL(glDrawBuffer);CORE_GL(glClearDepth);CORE_GL(glDrawArrays);CORE_GL(glDrawElements);CORE_GL(glPointSize);CORE_GL(glClipPlane);CORE_GL(glScissor);CORE_GL(glBlendFunc);CORE_GL(glColorMask);CORE_GL(glDepthFunc);CORE_GL(glTexSubImage1D);CORE_GL(glDepthMask);CORE_GL(glDisable);CORE_GL(glEnable);CORE_GL(glPolygonOffset);CORE_GL(glPolygonMode);
#include "gl_entrypoints.h"
#undef CORE_GL
    }
#undef GL_BIND
    return 0;
}
#undef GE6_GL_MS
}
#endif
