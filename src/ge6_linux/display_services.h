#pragma once
// Authored Windows display/GDI query transport to a real X11/RandR server.
// No monitor modes or vendor identities are synthesized.
#include "file_services.h"
#include <array>
#include <cmath>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#ifdef GE6_X11
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrandr.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#endif
namespace ge6_display {
#define GE6_DISPLAY_MS __attribute__((ms_abi))
inline void(*trace)(const char*)=nullptr;
inline void(*set_error)(uint32_t)=nullptr;
inline void(*stop)(const char*,const char*,uintptr_t)=nullptr;
struct Device {uint32_t size;uint16_t name[32],description[128];uint32_t flags;uint16_t id[128],key[128];};
static_assert(sizeof(Device)==840&&offsetof(Device,flags)==324);
inline std::recursive_mutex mutex;
template<class T>inline T field(const void* raw,size_t at){T value;std::memcpy(&value,static_cast<const uint8_t*>(raw)+at,sizeof(value));return value;}
template<class T>inline void store(void* raw,size_t at,T value){std::memcpy(static_cast<uint8_t*>(raw)+at,&value,sizeof(value));}
template<size_t N>inline void text(uint16_t(&out)[N],const std::u16string& value){std::fill(std::begin(out),std::end(out),0);std::copy_n(value.begin(),std::min(value.size(),N-1),out);}
inline std::u16string wide(const uint16_t* raw){std::u16string out;if(raw)for(size_t i=0;i<256&&raw[i];++i)out.push_back(char16_t(raw[i]));return out;}
#ifdef GE6_X11
inline Display* display=nullptr;
struct Mode {unsigned long id;int width,height,rate;};
struct Output {std::u16string name,monitor,description;unsigned long id,crtc;int x,y,width,height,mm_width,mm_height;bool primary;Mode current;std::vector<Mode> modes;};
inline std::vector<Output> outputs;
inline int bits=0;
inline bool connect(){
    if(display)return true;
    static bool initialized=XInitThreads()!=0;if(!initialized){set_error(50);return false;}
    display=XOpenDisplay(nullptr);if(!display){set_error(1167);return false;}
    int n=0;auto* formats=XListPixmapFormats(display,&n);for(int i=0;i<n;++i)if(formats[i].depth==DefaultDepth(display,DefaultScreen(display)))bits=formats[i].bits_per_pixel;XFree(formats);
    return true;
}
inline Mode mode(const XRRModeInfo& info){double hz=info.hTotal&&info.vTotal?double(info.dotClock)/(double(info.hTotal)*info.vTotal):0;if(info.modeFlags&RR_Interlace)hz*=2;if(info.modeFlags&RR_DoubleScan)hz/=2;return {info.id,int(info.width),int(info.height),int(std::round(hz))};}
inline bool refresh(){
    outputs.clear();if(!connect())return false;auto root=DefaultRootWindow(display);auto* resources=XRRGetScreenResourcesCurrent(display,root);if(!resources){set_error(50);return false;}auto primary=XRRGetOutputPrimary(display,root);
    for(int i=0;i<resources->noutput;++i){auto* info=XRRGetOutputInfo(display,resources,resources->outputs[i]);if(!info)continue;
        if(info->connection==RR_Connected&&info->crtc){auto* crtc=XRRGetCrtcInfo(display,resources,info->crtc);if(crtc&&crtc->mode){Output output{};output.id=resources->outputs[i];output.crtc=info->crtc;output.x=crtc->x;output.y=crtc->y;output.width=int(crtc->width);output.height=int(crtc->height);output.mm_width=int(info->mm_width);output.mm_height=int(info->mm_height);output.primary=output.id==primary;
            output.name=u"\\\\.\\DISPLAY"+ge6_fs::utf16(std::to_string(outputs.size()+1));output.monitor=output.name+u"\\Monitor0";output.description=u"X11 RandR "+ge6_fs::utf16(std::string(info->name,size_t(info->nameLen)));
            for(int j=0;j<info->nmode;++j)for(int k=0;k<resources->nmode;++k)if(info->modes[j]==resources->modes[k].id){auto value=mode(resources->modes[k]);output.modes.push_back(value);if(value.id==crtc->mode)output.current=value;}
            outputs.push_back(std::move(output));}if(crtc)XRRFreeCrtcInfo(crtc);
        }XRRFreeOutputInfo(info);
    }XRRFreeScreenResources(resources);if(!outputs.empty()&&!primary)outputs.front().primary=true;return !outputs.empty();
}
inline Output* output(const uint16_t* raw){if(outputs.empty()&&!refresh())return nullptr;auto name=wide(raw);for(auto& value:outputs)if(!raw||name==value.name||name==value.monitor)return &value;return nullptr;}
#endif
inline GE6_DISPLAY_MS int devices(const uint16_t* parent,uint32_t index,Device* result,uint32_t flags){
    trace("EnumDisplayDevicesW");if(!result||result->size!=840){set_error(87);return 0;}if(flags){stop("USER32.dll","EnumDisplayDevicesW",reinterpret_cast<uintptr_t>(__builtin_return_address(0)));return 0;}
#ifdef GE6_X11
    std::lock_guard<std::recursive_mutex> lock(mutex);if(!parent&&index==0&&!refresh())return 0;if(!connect())return 0;
    Output* value=nullptr;bool monitor=parent!=nullptr;
    if(!parent){if(index<outputs.size())value=&outputs[index];}
    else if(index==0)for(auto& item:outputs)if(wide(parent)==item.name)value=&item;
    if(!value)return 0;
    std::memset(result,0,sizeof(*result));result->size=840;text(result->name,monitor?value->monitor:value->name);text(result->description,value->description);result->flags=1|(value->primary?4:0);text(result->id,u"X11-RANDR:"+ge6_fs::utf16(std::to_string(value->id)));return 1;
#else
    (void)parent;(void)index;stop("USER32.dll","EnumDisplayDevicesW",reinterpret_cast<uintptr_t>(__builtin_return_address(0)));return 0;
#endif
}
inline GE6_DISPLAY_MS int settings(const uint16_t* name,uint32_t index,void* result){
    trace("EnumDisplaySettingsW");if(!result||field<uint16_t>(result,68)!=220||field<uint16_t>(result,70)!=0){set_error(87);return 0;}
#ifdef GE6_X11
    std::lock_guard<std::recursive_mutex> lock(mutex);auto* value=output(name);if(!value)return 0;Mode selected{};
    if(index==0xffffffffu)selected=value->current;else if(index<value->modes.size())selected=value->modes[index];else return 0;
    std::memset(result,0,220);auto label=value->name;std::memcpy(result,label.data(),std::min(label.size(),size_t(31))*2);store<uint16_t>(result,64,0x401);store<uint16_t>(result,68,220);store<uint32_t>(result,72,0x7c0020);store<int32_t>(result,76,value->x);store<int32_t>(result,80,value->y);store<uint32_t>(result,168,uint32_t(bits));store<uint32_t>(result,172,uint32_t(selected.width));store<uint32_t>(result,176,uint32_t(selected.height));store<uint32_t>(result,184,uint32_t(selected.rate));return 1;
#else
    (void)name;(void)index;stop("USER32.dll","EnumDisplaySettingsW",reinterpret_cast<uintptr_t>(__builtin_return_address(0)));return 0;
#endif
}
#ifdef GE6_X11
struct DC {Window drawable;GC gc;int width,height,mm_width,mm_height;uintptr_t window=0;};
struct Bitmap {Pixmap drawable;int width,height;};
inline uintptr_t next=0x75000000;
inline std::map<uintptr_t,DC> dcs;
inline std::map<uintptr_t,Bitmap> bitmaps;
inline GE6_DISPLAY_MS uintptr_t create_dc(const uint16_t* driver,const uint16_t*,const uint16_t*,const void*){trace("CreateDCW");std::lock_guard<std::recursive_mutex> lock(mutex);auto* value=output(driver);if(!value)return 0;auto root=DefaultRootWindow(display);auto gc=XCreateGC(display,root,0,nullptr);if(!gc){set_error(8);return 0;}auto handle=next++;dcs.emplace(handle,DC{root,gc,value->width,value->height,value->mm_width,value->mm_height});return handle;}
inline GE6_DISPLAY_MS int caps(uintptr_t handle,int index){trace("GetDeviceCaps");std::lock_guard<std::recursive_mutex> lock(mutex);auto found=dcs.find(handle);if(found==dcs.end()){set_error(6);return 0;}auto& value=found->second;switch(index){case 8:return value.width;case 10:return value.height;case 12:return bits;case 14:return 1;case 4:return value.mm_width;case 6:return value.mm_height;default:stop("GDI32.dll","GetDeviceCaps",reinterpret_cast<uintptr_t>(__builtin_return_address(0)));return 0;}}
inline GE6_DISPLAY_MS int delete_dc(uintptr_t handle){trace("DeleteDC");std::lock_guard<std::recursive_mutex> lock(mutex);auto found=dcs.find(handle);if(found==dcs.end()){set_error(6);return 0;}XFreeGC(display,found->second.gc);dcs.erase(found);return 1;}
inline GE6_DISPLAY_MS uintptr_t bitmap(uintptr_t handle,int width,int height){trace("CreateCompatibleBitmap");std::lock_guard<std::recursive_mutex> lock(mutex);auto found=dcs.find(handle);if(found==dcs.end()){set_error(6);return 0;}if(width<=0||height<=0||width>4096||height>4096){set_error(87);return 0;}auto pixmap=XCreatePixmap(display,found->second.drawable,unsigned(width),unsigned(height),unsigned(DefaultDepth(display,DefaultScreen(display))));if(!pixmap)return 0;auto token=next++;bitmaps.emplace(token,Bitmap{pixmap,width,height});return token;}
inline GE6_DISPLAY_MS int dibits(uintptr_t dc,uintptr_t handle,uint32_t,uint32_t lines,void* pixels,void* info,uint32_t usage){trace("GetDIBits");std::lock_guard<std::recursive_mutex> lock(mutex);auto found=bitmaps.find(handle);if(dcs.find(dc)==dcs.end()||found==bitmaps.end()){set_error(6);return 0;}if(!info||field<uint32_t>(info,0)!=40){set_error(87);return 0;}if(pixels||usage){stop("GDI32.dll","GetDIBits",reinterpret_cast<uintptr_t>(__builtin_return_address(0)));return 0;}auto* visual=DefaultVisual(display,DefaultScreen(display));std::memset(info,0,52);store<uint32_t>(info,0,40);store<int32_t>(info,4,found->second.width);store<int32_t>(info,8,found->second.height);store<uint16_t>(info,12,1);store<uint16_t>(info,14,uint16_t(bits));store<uint32_t>(info,16,3);store<uint32_t>(info,40,uint32_t(visual->red_mask));store<uint32_t>(info,44,uint32_t(visual->green_mask));store<uint32_t>(info,48,uint32_t(visual->blue_mask));return int(std::min(lines,uint32_t(found->second.height)));}
inline GE6_DISPLAY_MS int delete_object(uintptr_t handle){trace("DeleteObject");std::lock_guard<std::recursive_mutex> lock(mutex);auto found=bitmaps.find(handle);if(found==bitmaps.end()){set_error(6);return 0;}XFreePixmap(display,found->second.drawable);bitmaps.erase(found);return 1;}
#endif
inline uintptr_t resolve(const std::string& library,const std::string& name){
#define DISPLAY_BIND(n,f) if(name==n)return reinterpret_cast<uintptr_t>(&f)
    if(library=="USER32.dll"){DISPLAY_BIND("EnumDisplayDevicesW",devices);DISPLAY_BIND("EnumDisplaySettingsW",settings);}
#ifdef GE6_X11
    if(library=="GDI32.dll"){DISPLAY_BIND("CreateDCW",create_dc);DISPLAY_BIND("GetDeviceCaps",caps);DISPLAY_BIND("DeleteDC",delete_dc);DISPLAY_BIND("CreateCompatibleBitmap",bitmap);DISPLAY_BIND("GetDIBits",dibits);DISPLAY_BIND("DeleteObject",delete_object);}
#endif
#undef DISPLAY_BIND
    return 0;
}
#undef GE6_DISPLAY_MS
}
