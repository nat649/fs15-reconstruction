// Authored OS/ABI integration fixture. It contains no original game code.
#include "gl_services.h"
#include <stdexcept>
#include <dlfcn.h>
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
    const uint16_t ends[]{0,65535};require(ramp_sample(ends,2,1,3)==32768&&ramp_sample(ends,2,2,3)==65535&&ramp_sample(ends,2,0,1)==0,"Gamma resampling endpoints differ");
    std::array<uint16_t,770> gamma{};gamma.front()=0x1234;gamma.back()=0xabcd;auto display_dc=create_dc(adapter.name,nullptr,nullptr,nullptr);
    auto crtc=gamma_crtc(display_dc);auto* server_gamma=crtc?XRRGetCrtcGamma(display,crtc):nullptr;
    if(server_gamma&&server_gamma->size>0){require(get_gamma(display_dc,gamma.data()+1),"Actual RandR gamma read failed");for(size_t i=0;i<256;++i)require(gamma[i+1]==ramp_sample(server_gamma->red,size_t(server_gamma->size),i,256),"Gamma values differ from real server");require(set_gamma(display_dc,gamma.data()+1),"Actual unchanged gamma write/readback failed");}
    else require(!get_gamma(display_dc,gamma.data()+1)&&error==50,"Unavailable gamma reported supported");
    if(server_gamma)XRRFreeGamma(server_gamma);
    require(gamma.front()==0x1234&&gamma.back()==0xabcd&&!get_gamma(display_dc,nullptr)&&error==87&&!get_gamma(0,gamma.data()+1)&&error==6,"Gamma guards/invalid inputs differ");
    require(delete_dc(display_dc),"Display gamma DC leaked");
    ge6_classes::ClassW klass{};klass.procedure=procedure;klass.instance=ge6_images::module;klass.name=reinterpret_cast<const uint16_t*>(u"AuthoredX11Fixture");
    auto atom=ge6_classes::add(&klass);require(atom!=0,"Fixture class registration failed");
    ge6_windows::Rect normal{10,20,30,40},zero{10,20,10,40},reversed{30,20,10,40};
    require(!ge6_windows::rect_empty(&normal)&&ge6_windows::rect_empty(&zero)&&ge6_windows::rect_empty(&reversed),"Native cursor-clip rectangle emptiness differs");
    auto hwnd=ge6_windows::create(0,klass.name,reinterpret_cast<const uint16_t*>(u"Authored X11 fixture"),0,40,40,320,180,0,0,klass.instance,1234);
    require(hwnd&&created==1,"Native MS ABI creation/callback failed");
    auto cursor=ge6_windows::load_cursor(0,reinterpret_cast<const uint16_t*>(32512));require(cursor&&ge6_windows::set_cursor(cursor)==0&&ge6_windows::show_cursor(0)==-1&&ge6_windows::show_cursor(1)==0&&ge6_windows::set_cursor(0)==cursor,"Actual X11 cursor/count lifetime differs");
    auto icon=std::make_shared<ge6_images::Icon>();icon->width=1;icon->height=1;icon->rgba={12,34,56,255};ge6_images::icons[1234]=icon;
    require(ge6_windows::send_a(hwnd,0x80,1,1234)==0&&ge6_windows::send_a(hwnd,0x7f,1,0)==1234,"Icon message round trip failed");
    Atom actual_type=0;int actual_format=0;unsigned long count=0,remaining=0;unsigned char* property=nullptr;
    require(XGetWindowProperty(display,ge6_windows::windows.at(hwnd).xid,XInternAtom(display,"_NET_WM_ICON",False),0,3,False,XA_CARDINAL,&actual_type,&actual_format,&count,&remaining,&property)==Success&&actual_format==32&&count==3,"Real X11 icon property missing");
    auto* values=reinterpret_cast<unsigned long*>(property);require(actual_type==XA_CARDINAL&&values[0]==1&&values[1]==1&&uint32_t(values[2])==0xff0c2238u,"X11 icon pixels differ from original RGBA data");XFree(property);
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
    GLuint texture=0;ge6_gl::Bridge<&glGenTextures>::call(1,&texture);ge6_gl::Bridge<&glBindTexture>::call(GL_TEXTURE_2D,texture);
    const uint8_t texture_pixels[]{1,2,3,4,11,22,33,44,55,66,77,88,99,111,222,255};uint8_t copied[16]{};
    auto upload=reinterpret_cast<void(__attribute__((ms_abi))*)(GLenum,GLint,GLint,GLsizei,GLsizei,GLint,GLenum,GLenum,const void*)>(ge6_gl::procedure("glTexImage2D"));require(upload,"Texture ABI adapter absent");upload(GL_TEXTURE_2D,0,GL_RGBA8,2,2,0,GL_RGBA,GL_UNSIGNED_BYTE,texture_pixels);
    ge6_gl::Bridge<&glGetTexImage>::call(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,copied);require(std::memcmp(copied,texture_pixels,16)==0,"Nine-argument texture ABI/readback differs");ge6_gl::Bridge<&glDeleteTextures>::call(1,&texture);
    auto vertex=ge6_gl::Bridge<&glCreateShader>::call(GL_VERTEX_SHADER);auto fragment=ge6_gl::Bridge<&glCreateShader>::call(GL_FRAGMENT_SHADER);
    const char* vs="#version 120\nattribute vec2 position;void main(){gl_Position=vec4(position,0.0,1.0);}";
    const char* fs="#version 120\nuniform vec4 color;void main(){gl_FragColor=color;}";
    ge6_gl::Bridge<&glShaderSource>::call(vertex,1,&vs,nullptr);ge6_gl::Bridge<&glShaderSource>::call(fragment,1,&fs,nullptr);ge6_gl::Bridge<&glCompileShader>::call(vertex);ge6_gl::Bridge<&glCompileShader>::call(fragment);
    GLint status=0;for(auto shader:{vertex,fragment}){ge6_gl::Bridge<&glGetShaderiv>::call(shader,GL_COMPILE_STATUS,&status);require(status,"Actual shader compile failed");}
    auto program=ge6_gl::Bridge<&glCreateProgram>::call();ge6_gl::Bridge<&glAttachShader>::call(program,vertex);ge6_gl::Bridge<&glAttachShader>::call(program,fragment);ge6_gl::Bridge<&glBindAttribLocation>::call(program,0,"position");ge6_gl::Bridge<&glLinkProgram>::call(program);ge6_gl::Bridge<&glGetProgramiv>::call(program,GL_LINK_STATUS,&status);require(status,"Actual shader link failed");ge6_gl::Bridge<&glUseProgram>::call(program);
    auto uniform=ge6_gl::Bridge<&glGetUniformLocation>::call(program,"color");require(uniform>=0,"Actual uniform missing");
    auto set_color=reinterpret_cast<void(__attribute__((ms_abi))*)(GLint,GLfloat,GLfloat,GLfloat,GLfloat)>(ge6_gl::procedure("glUniform4f"));require(set_color,"Mixed float/integer ABI adapter absent");set_color(uniform,0.25f,0.5f,0.75f,1.0f);
    GLuint buffer=0;const float positions[]{-1,-1,3,-1,-1,3};ge6_gl::Bridge<&glGenBuffersARB>::call(1,&buffer);ge6_gl::Bridge<&glBindBufferARB>::call(GL_ARRAY_BUFFER_ARB,buffer);ge6_gl::Bridge<&glBufferDataARB>::call(GL_ARRAY_BUFFER_ARB,sizeof(positions),positions,GL_STATIC_DRAW_ARB);
    ge6_gl::Bridge<&glEnableVertexAttribArray>::call(0);ge6_gl::Bridge<&glVertexAttribPointer>::call(0,2,GL_FLOAT,GL_FALSE,0,nullptr);clear(0,0,0,1);ge6_gl::Bridge<&glClear>::call(GL_COLOR_BUFFER_BIT);ge6_gl::Bridge<&glDrawArrays>::call(GL_TRIANGLES,0,3);
    ge6_gl::Bridge<&glReadPixels>::call(0,0,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);require(pixel[0]>=63&&pixel[0]<=65&&pixel[1]>=127&&pixel[1]<=129&&pixel[2]>=190&&pixel[2]<=192&&ge6_gl::Bridge<&glGetError>::call()==GL_NO_ERROR,"Native shader/buffer/draw ABI pixel differs");
    ge6_gl::Bridge<&glDisableVertexAttribArray>::call(0);ge6_gl::Bridge<&glBindBufferARB>::call(GL_ARRAY_BUFFER_ARB,0);ge6_gl::Bridge<&glDeleteBuffersARB>::call(1,&buffer);ge6_gl::Bridge<&glUseProgram>::call(0);ge6_gl::Bridge<&glDeleteProgram>::call(program);ge6_gl::Bridge<&glDeleteShader>::call(vertex);ge6_gl::Bridge<&glDeleteShader>::call(fragment);
    require(!ge6_gl::procedure("glDebugMessageCallbackARB")&&!ge6_gl::procedure("AuthoredAbsentGLFunction"),"Unsafe/unavailable GL callback exposed");
    ge6_windows::show(hwnd,5);XWindowAttributes attributes{};require(XGetWindowAttributes(display,ge6_windows::windows.at(hwnd).xid,&attributes)&&attributes.map_state==IsViewable,"Host window is not actually mapped");
    require(ge6_windows::focus()==hwnd,"Activating ShowWindow did not acquire actual X focus");
    ge6_windows::Message activated{};
    require(ge6_windows::peek(&activated,hwnd,6,6,1)&&activated.wparam==1,"Real X FocusIn did not activate the original window procedure");
    require(!ge6_windows::peek(&activated,hwnd,6,6,1),"Focus activation was duplicated");
    XSetInputFocus(display,PointerRoot,RevertToPointerRoot,CurrentTime);XSync(display,False);
    require(ge6_windows::peek(&activated,hwnd,6,6,1)&&activated.wparam==0&&ge6_windows::focus()==0,"Real focus loss did not deactivate the window");
    ge6_windows::show(hwnd,8);require(ge6_windows::focus()==0&&!ge6_windows::peek(&activated,hwnd,6,6,1),"SW_SHOWNA stole focus or invented activation");
    ge6_windows::show(hwnd,5);require(ge6_windows::focus()==hwnd&&ge6_windows::peek(&activated,hwnd,6,6,1)&&activated.wparam==1,"Window did not reactivate through the X server");
    auto xtst=dlopen("libXtst.so.6",RTLD_NOW);require(xtst,"Real XTest input library unavailable");
    using ButtonTest=int(*)(Display*,unsigned,int,unsigned long);
    auto button_test=reinterpret_cast<ButtonTest>(dlsym(xtst,"XTestFakeButtonEvent"));require(button_test,"XTest button input unavailable");
    for(auto pair:{std::pair<unsigned,int>{1,1},{2,4},{3,2}}){
        require(button_test(display,pair.first,True,0),"Cannot press actual XTest button");XSync(display,False);
        require(uint16_t(ge6_windows::async_key_state(pair.second))==0x8001u&&uint16_t(ge6_windows::async_key_state(pair.second))==0x8000u,"Async mouse query lost actual state or repeated a press transition");
        require(button_test(display,pair.first,False,0),"Cannot release actual XTest button");XSync(display,False);
        require(ge6_windows::async_key_state(pair.second)==0,"Released XTest button remained down");
    }
    unsigned char original_map[256]{};int map_count=XGetPointerMapping(display,original_map,256);
    require(map_count>=3,"X pointer has fewer than three physical buttons");
    std::array<unsigned char,256> swapped{};std::copy_n(original_map,map_count,swapped.begin());std::swap(swapped[0],swapped[2]);
    require(XSetPointerMapping(display,swapped.data(),map_count)==MappingSuccess&&button_test(display,1,True,0),"Cannot exercise actual swapped pointer mapping");XSync(display,False);
    require((uint16_t(ge6_windows::async_key_state(1))&0x8000u)&&!(uint16_t(ge6_windows::async_key_state(2))&0x8000u),"Async query returned logical rather than physical mouse-button state");
    require(button_test(display,1,False,0),"Cannot release swapped physical button");XSync(display,False);
    require(XSetPointerMapping(display,original_map,map_count)==MappingSuccess,"Cannot restore actual pointer mapping");
    auto key_test=reinterpret_cast<ButtonTest>(dlsym(xtst,"XTestFakeKeyEvent"));require(key_test,"XTest keyboard input unavailable");
    auto press_key=[&](KeySym sym,bool down){auto key=XKeysymToKeycode(display,sym);require(key&&key_test(display,key,down,0),"Cannot deliver actual XTest key");XSync(display,False);};
    auto next_key=[&](){ge6_windows::Message m{};require(ge6_windows::peek(&m,hwnd,0x100,0x105,1),"Real X key message lost");return m;};
    std::array<uint8_t,258> state{};state.front()=0x12;state.back()=0x34;
    // Both events are already in the X queue: synchronous state must still
    // change only when each Windows message is removed, not when polled.
    press_key(XK_w,true);press_key(XK_w,false);
    ge6_windows::Message pending{};require(ge6_windows::peek(&pending,hwnd,0x100,0x105,0)&&!ge6_windows::key_state('W'),"PM_NOREMOVE advanced keyboard state");
    auto kd=next_key();require(kd.message==0x100&&kd.wparam=='W'&&uint32_t(kd.lparam)==0x00110001u&&kd.time>0,"W key virtual/physical scan or transition differs");
    require(ge6_windows::keyboard_snapshot(state.data()+1)&&state['W'+1]==0x80&&state.front()==0x12&&state.back()==0x34,"Keyboard snapshot or 256-byte guard differs");
    uint16_t chars[]{0x1234,0,0xabcd};require(ge6_windows::to_unicode('W',17,state.data()+1,chars+1,1,0)==1&&chars[1]=='w'&&chars[0]==0x1234&&chars[2]==0xabcd,"Actual layout translation/UTF16 bounds differ");
    auto ku=next_key();require(ku.message==0x101&&uint32_t(ku.lparam)==0xc0110001u&&!ge6_windows::key_state('W'),"Key release transition/synchronous state differs");
    press_key(XK_Shift_R,true);kd=next_key();require(kd.wparam==0x10&&uint32_t(kd.lparam)==0x00360001u&&ge6_windows::key_state(0xa1)&&ge6_windows::key_state(0x10)&&!ge6_windows::key_state(0xa0),"Right/aggregate Shift differs");
    press_key(XK_w,true);kd=next_key();ge6_windows::keyboard_snapshot(state.data()+1);require(ge6_windows::to_unicode('W',17,state.data()+1,chars+1,1,0)==1&&chars[1]=='W',"Real Shift layout text differs");
    require(uint16_t(ge6_windows::async_key_state('W'))==0x8001u,"Async keyboard did not query actual held key");
    press_key(XK_w,false);next_key();press_key(XK_Shift_R,false);next_key();
    press_key(XK_Caps_Lock,true);next_key();press_key(XK_Caps_Lock,false);next_key();
    require(ge6_windows::key_state(20)==1,"Caps Lock toggle was lost on release");ge6_windows::keyboard_snapshot(state.data()+1);
    require(ge6_windows::to_unicode('W',17,state.data()+1,chars+1,1,0)==1&&chars[1]=='W',"Caps Lock text differs from X layout");
    press_key(XK_Caps_Lock,true);next_key();press_key(XK_Caps_Lock,false);next_key();require(!ge6_windows::key_state(20),"Caps Lock did not toggle off");
    press_key(XK_w,true);next_key();usleep(900000);XSync(display,False);
    bool repeated=false;while(ge6_windows::peek(&pending,hwnd,0x100,0x105,1)){require(pending.message==0x100&&pending.wparam=='W'&&(uint32_t(pending.lparam)&0x40000000u),"Autorepeat lost held-state bit or invented key release");repeated=true;}
    require(repeated,"Actual X server autorepeat was not delivered");press_key(XK_w,false);next_key();
    press_key(XK_Up,true);kd=next_key();require(kd.wparam==0x26&&uint32_t(kd.lparam)==0x01480001u,"Extended arrow scan differs");press_key(XK_Up,false);next_key();
    press_key(XK_Control_R,true);kd=next_key();require(kd.wparam==0x11&&ge6_windows::key_state(0xa3)&&!ge6_windows::key_state(0xa2),"Extended right Control state differs");press_key(XK_Control_R,false);next_key();
    press_key(XK_Alt_L,true);kd=next_key();require(kd.message==0x104&&kd.wparam==0x12,"Alt key did not produce system key message");
    press_key(XK_w,true);kd=next_key();require(kd.message==0x104&&(uint32_t(kd.lparam)&0x20000000u),"Alt context/system key bit missing");press_key(XK_w,false);next_key();press_key(XK_Alt_L,false);next_key();
    press_key(XK_Return,true);kd=next_key();ge6_windows::keyboard_snapshot(state.data()+1);require(kd.wparam==13&&ge6_windows::to_unicode(13,28,state.data()+1,chars+1,1,0)==1&&chars[1]==13,"Enter transport/text differs");press_key(XK_Return,false);next_key();
    require(ge6_windows::map_key(0x26,4)==0xe048&&ge6_windows::map_key(0xe01d,3)==0xa3,"Extended MapVirtualKey differs");
    require(!ge6_windows::keyboard_snapshot(nullptr)&&error==87&&!ge6_windows::to_unicode('W',17,state.data()+1,chars+1,0,0)&&error==87,"Invalid keyboard buffer accepted");
    // Holding a real key across focus loss must not leave GE6 moving forever.
    press_key(XK_w,true);next_key();XSetInputFocus(display,PointerRoot,RevertToPointerRoot,CurrentTime);XSync(display,False);
    ku=next_key();require(ku.message==0x101&&ku.wparam=='W'&&!ge6_windows::key_state('W'),"Focus loss retained held keyboard state");
    press_key(XK_w,false);ge6_windows::show(hwnd,5);
    dlclose(xtst);
    while(ge6_windows::peek(&activated,hwnd,0x200,0x20e,1)){} // Consume the actual XTest button messages before separate transport fixtures.
    require(!ge6_windows::async_key_state(5)&&error==50&&!ge6_windows::clipboard_sequence_unavailable()&&error==50,"Unavailable extra-button/clipboard access reported data");
    ge6_windows::Rect damage{};require(ge6_windows::update_rect(hwnd,&damage,0)&&damage.right==320&&damage.bottom==180,"Real initial X11 exposure was lost");
    require(ge6_windows::validate_rect(hwnd,nullptr)&&!ge6_windows::update_rect(hwnd,&damage,0)&&damage.left==0&&damage.right==0,"Validated exposure retained damage");
    XClearArea(display,ge6_windows::windows.at(hwnd).xid,10,20,40,30,True);XSync(display,False);
    require(ge6_windows::update_rect(hwnd,&damage,0)&&damage.left==10&&damage.top==20&&damage.right==50&&damage.bottom==50,"Actual X11 damaged rectangle differs");
    ge6_windows::Rect partial{10,20,30,50};require(ge6_windows::validate_rect(hwnd,&partial)&&ge6_windows::update_rect(hwnd,&damage,0)&&damage.left==30&&damage.right==50,"Partial validation discarded unpainted area");
    require(ge6_windows::validate_rect(hwnd,nullptr)&&!ge6_windows::update_rect(hwnd,nullptr,0)&&!ge6_windows::update_rect(0,&damage,0)&&error==1400,"Update rectangle lifetime/invalid handle differs");
    auto pointer_event=[&](int type,unsigned button,unsigned state,int x,int y,bool expected=true){
        XEvent event{};auto xid=ge6_windows::windows.at(hwnd).xid;
        if(type==MotionNotify){event.xmotion.type=type;event.xmotion.display=display;event.xmotion.window=xid;event.xmotion.root=DefaultRootWindow(display);event.xmotion.x=x;event.xmotion.y=y;event.xmotion.x_root=x+40;event.xmotion.y_root=y+40;event.xmotion.state=state;event.xmotion.time=1234;event.xmotion.same_screen=True;}
        else{event.xbutton.type=type;event.xbutton.display=display;event.xbutton.window=xid;event.xbutton.root=DefaultRootWindow(display);event.xbutton.x=x;event.xbutton.y=y;event.xbutton.x_root=x+40;event.xbutton.y_root=y+40;event.xbutton.state=state;event.xbutton.button=button;event.xbutton.time=1234;event.xbutton.same_screen=True;}
        auto mask=type==MotionNotify?PointerMotionMask:(type==ButtonPress?ButtonPressMask:ButtonReleaseMask);
        require(XSendEvent(display,xid,False,mask,&event),"Cannot deliver authored pointer event through actual X server");XSync(display,False);
        ge6_windows::Message result{};require(bool(ge6_windows::peek(&result,hwnd,0x200,0x20e,1))==expected,"Pointer event lost or ignored wheel release queued");return result;
    };
    auto motion=pointer_event(MotionNotify,0,ShiftMask|Button1Mask,-4,17);require(motion.message==0x200&&motion.wparam==5&&int16_t(motion.lparam&65535)==-4&&int16_t((uintptr_t(motion.lparam)>>16)&65535)==17&&motion.point.x==36&&motion.point.y==57&&motion.time==1234,"Motion client/root coordinates, modifier or timestamp differs");
    auto press=pointer_event(ButtonPress,1,ShiftMask|ControlMask,10,20);require(press.message==0x201&&press.wparam==13,"Left press did not include post-transition button/modifier state");
    auto release=pointer_event(ButtonRelease,1,Button1Mask|ControlMask,10,20);require(release.message==0x202&&release.wparam==8,"Left release retained released button");
    auto middle=pointer_event(ButtonPress,2,0,10,20);require(middle.message==0x207&&middle.wparam==16,"Middle button mapping differs");
    auto right=pointer_event(ButtonRelease,3,Button2Mask|Button3Mask,10,20);require(right.message==0x205&&right.wparam==16,"Right release/middle held mapping differs");
    auto extra=pointer_event(ButtonPress,8,Button1Mask,10,20);require(extra.message==0x20b&&extra.wparam==0x10021,"X1 button ID/state differs");
    motion=pointer_event(MotionNotify,0,Button1Mask,11,21);require(motion.wparam==0x21,"Held extra button lost on subsequent motion");
    extra=pointer_event(ButtonRelease,8,Button1Mask,10,20);require(extra.message==0x20c&&extra.wparam==0x10001,"X1 release retained extra button");
    auto wheel=pointer_event(ButtonPress,5,ControlMask|Button1Mask,-5,8);require(wheel.message==0x20a&&int16_t(wheel.wparam>>16)==-120&&(wheel.wparam&65535)==9&&int16_t(wheel.lparam&65535)==35&&int16_t((uintptr_t(wheel.lparam)>>16)&65535)==48,"Wheel delta, held state or screen coordinates differ");
    pointer_event(ButtonRelease,5,0,10,20,false);
    wheel=pointer_event(ButtonPress,7,0,10,20);require(wheel.message==0x20e&&int16_t(wheel.wparam>>16)==120,"Horizontal wheel direction differs");
    pointer_event(ButtonPress,10,0,10,20,false);
    ge6_windows::MouseTracking tracking{24,2,hwnd,0xffffffffu,0};
    XWarpPointer(display,0,ge6_windows::windows.at(hwnd).xid,0,0,0,0,20,20);XSync(display,False);
    require(ge6_windows::track_mouse(&tracking)&&ge6_windows::windows.at(hwnd).mouse_leave,"Real inside pointer did not arm leave tracking");
    XWarpPointer(display,0,DefaultRootWindow(display),0,0,0,0,1000,650);XSync(display,False);
    ge6_windows::Message leave{};require(ge6_windows::peek(&leave,hwnd,0x2a3,0x2a3,1)&&leave.wparam==0&&leave.lparam==0&&!ge6_windows::windows.at(hwnd).mouse_leave,"Actual X11 leave did not produce one-shot WM_MOUSELEAVE");
    require(!ge6_windows::peek(&leave,hwnd,0x2a3,0x2a3,1),"Leave tracking generated duplicate leave messages");
    require(ge6_windows::track_mouse(&tracking)&&ge6_windows::peek(&leave,hwnd,0x2a3,0x2a3,1),"Already-outside registration did not queue immediate leave");
    XWarpPointer(display,0,ge6_windows::windows.at(hwnd).xid,0,0,0,0,20,20);XSync(display,False);require(ge6_windows::track_mouse(&tracking),"Cannot rearm client leave tracking");
    tracking.flags=0x80000002u;require(ge6_windows::track_mouse(&tracking),"Cannot cancel client leave tracking");
    XWarpPointer(display,0,DefaultRootWindow(display),0,0,0,0,1000,650);XSync(display,False);require(!ge6_windows::peek(&leave,hwnd,0x2a3,0x2a3,1),"Cancelled tracking still generated leave");
    tracking.size=23;require(!ge6_windows::track_mouse(&tracking)&&error==87,"Invalid tracking ABI size accepted");tracking.size=24;tracking.flags=1;require(!ge6_windows::track_mouse(&tracking)&&error==50,"Unimplemented hover tracking reported success");tracking.flags=2;tracking.window=0;require(!ge6_windows::track_mouse(&tracking)&&error==1400,"Invalid tracked window accepted");
    ge6_windows::RawInputDevice raw_mouse{1,2,1,0};require(ge6_windows::register_raw_input(&raw_mouse,1,16)&&ge6_windows::register_raw_input(&raw_mouse,1,16),"Removing absent raw mouse subscription is not idempotent");
    raw_mouse.target=hwnd;require(!ge6_windows::register_raw_input(&raw_mouse,1,16)&&error==87,"RIDEV_REMOVE accepted a non-null target");raw_mouse.target=0;raw_mouse.flags=0;
    require(!ge6_windows::register_raw_input(&raw_mouse,1,16)&&error==50,"Unavailable raw mouse registration reported success");
    require(!ge6_windows::register_raw_input(&raw_mouse,1,15)&&error==87&&!ge6_windows::register_raw_input(nullptr,1,16)&&error==87,"Invalid raw-input ABI accepted");
    ge6_windows::Point before_release{},after_release{};require(ge6_windows::cursor_position(&before_release)&&ge6_windows::clip_cursor(nullptr)&&ge6_windows::cursor_position(&after_release)&&before_release.x==after_release.x&&before_release.y==after_release.y,"Releasing empty cursor confinement warped the pointer");
    ge6_windows::Rect unsupported_clip{0,0,20,20};require(!ge6_windows::clip_cursor(&unsupported_clip)&&error==50,"Unavailable cursor confinement reported success");
    require(ge6_gl::make_current(0,0)&&ge6_gl::remove(context)&&ge6_windows::release_dc(hwnd,dc)&&ge6_windows::destroy(hwnd)&&ge6_classes::remove(klass.name,klass.instance),"Resource cleanup failed");
    require(ge6_windows::windows.empty()&&ge6_gl::contexts.empty(),"Native host resources leaked");
    std::puts("Authored X11/RandR/window/MS-ABI/GLX actual pixel and lifetime fixture passed; no game result claimed");return 0;
}catch(const std::exception& exception){std::fprintf(stderr,"X11 fixture failed: %s\n",exception.what());return 1;}}
