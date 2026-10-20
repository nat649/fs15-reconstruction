// Authored OS/ABI integration fixture. It contains no original game code.
#include "gl_services.h"
#include <stdexcept>
namespace {
uint32_t error=0;
unsigned created=0;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void traced(const char*){}
[[noreturn]]void unsupported(const char*,const char* name,uintptr_t){throw std::runtime_error(name);}
__attribute__((ms_abi)) int64_t procedure(uintptr_t handle,uint32_t message,uintptr_t wp,intptr_t lp){
    if(message==0x81){auto* info=reinterpret_cast<ge6_windows::Create*>(lp);require(info->width==320&&info->height==180&&info->parameter==1234,"CreateWindowExW stack arguments/callback differ");++created;}
    return ge6_windows::default_proc(handle,message,wp,lp);
}
}
int main(){try{
    using namespace ge6_display;
    trace=traced;set_error=[](uint32_t value){error=value;};stop=unsupported;
    ge6_classes::trace=traced;ge6_classes::set_error=set_error;ge6_classes::stop=[](const char* name,uintptr_t){throw std::runtime_error(name);};ge6_classes::record=[](const char*,uint16_t,uintptr_t){};
    Device adapter{};adapter.size=sizeof(adapter);require(devices(nullptr,0,&adapter,0)&&adapter.flags&1,"Real RandR display unavailable");
    std::array<uint8_t,224> settings_buffer{};store<uint16_t>(settings_buffer.data(),68,220);settings_buffer[220]=0xa5;
    require(settings(adapter.name,0xffffffffu,settings_buffer.data())&&field<uint32_t>(settings_buffer.data(),172)>0&&settings_buffer[220]==0xa5,"Display mode layout or guard differs");
    require(change_settings(adapter.name,settings_buffer.data(),0,4,nullptr)==0,"Already current real RandR mode rejected");
    store<uint32_t>(settings_buffer.data(),172,12345);require(change_settings(adapter.name,settings_buffer.data(),0,2,nullptr)==-2,"Absent display mode reported supported");
    require(change_settings(adapter.name,nullptr,0,0x80000000,nullptr)==-4,"Unsupported display flags accepted");
    ge6_classes::ClassW klass{};klass.procedure=procedure;klass.instance=ge6_images::module;klass.name=reinterpret_cast<const uint16_t*>(u"AuthoredX11Fixture");
    auto atom=ge6_classes::add(&klass);require(atom!=0,"Fixture class registration failed");
    auto hwnd=ge6_windows::create(0,klass.name,reinterpret_cast<const uint16_t*>(u"Authored X11 fixture"),0,40,40,320,180,0,0,klass.instance,1234);
    require(hwnd&&created==1,"Native MS ABI creation/callback failed");
    require(ge6_windows::set_property(hwnd,reinterpret_cast<const uint16_t*>(u"Test"),42)&&ge6_windows::get_property(hwnd,reinterpret_cast<const uint16_t*>(u"Test"))==42,"Window property round trip failed");
    require(!ge6_classes::remove(klass.name,klass.instance)&&error==1412,"Live window did not retain class");
    auto dc=ge6_windows::get_dc(hwnd);require(dc,"Window DC unavailable");
    uint8_t pfd[40]{};store<uint16_t>(pfd,0,40);store<uint16_t>(pfd,2,1);store<uint32_t>(pfd,4,0x25);pfd[9]=24;pfd[23]=16;
    auto format=ge6_gl::choose(dc,pfd);require(format&&ge6_gl::set_pixel(dc,format,pfd),"Actual GLX format unavailable");
    auto context=ge6_gl::create(dc);require(context&&ge6_gl::make_current(dc,context),"Actual GLX context unavailable");
    auto clear=reinterpret_cast<void(__attribute__((ms_abi))*)(float,float,float,float)>(ge6_gl::procedure("glClearColor"));
    require(clear,"Typed MS ABI GL export unavailable");clear(0.25f,0.5f,0.75f,1.0f);ge6_gl::Bridge<&glClear>::call(GL_COLOR_BUFFER_BIT);
    uint8_t pixel[4]{};ge6_gl::Bridge<&glReadPixels>::call(0,0,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
    require(pixel[0]>=63&&pixel[0]<=65&&pixel[1]>=127&&pixel[1]<=129&&pixel[2]>=190&&pixel[2]<=192,"GL floating point ABI or actual pixel readback differs");
    ge6_windows::show(hwnd,5);XWindowAttributes attributes{};require(XGetWindowAttributes(display,ge6_windows::windows.at(hwnd).xid,&attributes)&&attributes.map_state==IsViewable,"Host window is not actually mapped");
    require(ge6_gl::make_current(0,0)&&ge6_gl::remove(context)&&ge6_windows::release_dc(hwnd,dc)&&ge6_windows::destroy(hwnd)&&ge6_classes::remove(klass.name,klass.instance),"Resource cleanup failed");
    require(ge6_windows::windows.empty()&&ge6_gl::contexts.empty(),"Native host resources leaked");
    std::puts("Authored X11/RandR/window/MS-ABI/GLX actual pixel and lifetime fixture passed; no game result claimed");return 0;
}catch(const std::exception& exception){std::fprintf(stderr,"X11 fixture failed: %s\n",exception.what());return 1;}}
