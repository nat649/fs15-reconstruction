#pragma once
// Authored Windows window/input ABI transport. X11 owns the actual resources.
#include "display_services.h"
#include "window_classes.h"
#include <deque>
#ifdef GE6_X11
namespace ge6_windows {
#define GE6_WIN_MS __attribute__((ms_abi))
using namespace ge6_display;
inline const std::array<uint16_t,128> scan_vk=[] {
    std::array<uint16_t,128> v{};
    v[1]=27;const char* digits="1234567890";for(int i=0;i<10;++i)v[2+i]=uint16_t(digits[i]);
    v[12]=0xbd;v[13]=0xbb;v[14]=8;v[15]=9;
    const char* a="QWERTYUIOP";for(int i=0;i<10;++i)v[16+i]=uint16_t(a[i]);
    v[26]=0xdb;v[27]=0xdd;v[28]=13;v[29]=0x11;
    a="ASDFGHJKL";for(int i=0;i<9;++i)v[30+i]=uint16_t(a[i]);
    v[39]=0xba;v[40]=0xde;v[41]=0xc0;v[42]=0x10;v[43]=0xdc;
    a="ZXCVBNM";for(int i=0;i<7;++i)v[44+i]=uint16_t(a[i]);
    v[51]=0xbc;v[52]=0xbe;v[53]=0xbf;v[54]=0x10;v[55]=0x6a;v[56]=0x12;v[57]=32;v[58]=20;
    for(int i=0;i<10;++i)v[59+i]=uint16_t(0x70+i);
    v[69]=0x90;v[70]=0x91;v[71]=0x67;v[72]=0x68;v[73]=0x69;v[74]=0x6d;v[75]=0x64;v[76]=0x65;v[77]=0x66;v[78]=0x6b;v[79]=0x61;v[80]=0x62;v[81]=0x63;v[82]=0x60;v[83]=0x6e;v[86]=0xe2;v[87]=0x7a;v[88]=0x7b;
    return v;
}();
inline GE6_WIN_MS uint32_t map_key(uint32_t code,uint32_t type){
    trace("MapVirtualKeyW");std::lock_guard<std::recursive_mutex> lock(mutex);
    if(type==1)return code<scan_vk.size()?scan_vk[code]:0;
    if(type==0){for(size_t i=0;i<scan_vk.size();++i)if(scan_vk[i]==code)return uint32_t(i);return 0;}
    if(type!=2){stop("USER32.dll","MapVirtualKeyW",reinterpret_cast<uintptr_t>(__builtin_return_address(0)));return 0;}
    if(!connect())return 0;
    if(code>='A'&&code<='Z')return code; // MAPVK_VK_TO_CHAR always returns uppercase Latin letters.
    for(size_t i=0;i<scan_vk.size();++i)if(scan_vk[i]==code){auto key=XkbKeycodeToKeysym(display,KeyCode(i+8),0,0);if(key>=32&&key<=126)return uint32_t(key);if((key&0xff000000)==0x01000000)return uint32_t(key&0xffffff);}
    return 0;
}
inline std::map<uintptr_t,Cursor> cursors;
inline GE6_WIN_MS uintptr_t load_cursor(uintptr_t instance,const uint16_t* name){
    trace("LoadCursorW");if(instance||reinterpret_cast<uintptr_t>(name)!=32512){stop("USER32.dll","LoadCursorW",reinterpret_cast<uintptr_t>(__builtin_return_address(0)));return 0;}
    std::lock_guard<std::recursive_mutex> lock(mutex);if(!connect())return 0;auto found=cursors.find(32512);if(found!=cursors.end())return found->second;auto cursor=XCreateFontCursor(display,XC_left_ptr);cursors[32512]=cursor;return cursor;
}
inline GE6_WIN_MS uint32_t double_click(){
    trace("GetDoubleClickTime");std::lock_guard<std::recursive_mutex> lock(mutex);if(!connect())return 0;
    // Xlib's documented multiClickTime resource; 500 ms is this host adapter's default.
    const char* resource=XGetDefault(display,"GE6","multiClickTime");if(resource){char* end=nullptr;auto value=std::strtoul(resource,&end,10);if(end&&!*end&&value>0&&value<=5000)return uint32_t(value);}
    return 500;
}
struct Rect {int32_t left,top,right,bottom;};
inline GE6_WIN_MS int rect_empty(const Rect* rect){trace("IsRectEmpty");if(!rect){stop("USER32.dll","IsRectEmpty(null)",reinterpret_cast<uintptr_t>(__builtin_return_address(0)));return 0;}return rect->right<=rect->left||rect->bottom<=rect->top;}
struct Point {int32_t x,y;};
struct Message {uintptr_t window;uint32_t message,pad;uintptr_t wparam;intptr_t lparam;uint32_t time;Point point;uint32_t pad2;};
static_assert(sizeof(Message)==48);
struct WindowData {Window xid;std::shared_ptr<ge6_classes::Class> klass;ge6_classes::Procedure procedure;uint32_t style,extended;int x,y,width,height;uintptr_t instance,parent,user=0;bool message_only=false;std::map<std::u16string,uintptr_t> properties;std::map<unsigned,std::pair<uintptr_t,std::shared_ptr<ge6_images::Icon>>> icons;std::vector<Rect> damaged;uint32_t mouse_buttons=0;bool mouse_leave=false;};
inline std::map<uintptr_t,WindowData> windows;
inline std::deque<Message> messages;
inline uintptr_t foreground=0;
inline Cursor current_cursor=0,blank_cursor=0;
inline thread_local int cursor_count=0;
inline void apply_cursor(){
    if(!blank_cursor){const char empty=0;auto pixmap=XCreateBitmapFromData(display,DefaultRootWindow(display),&empty,1,1);XColor color{};blank_cursor=XCreatePixmapCursor(display,pixmap,pixmap,&color,&color,0,0);XFreePixmap(display,pixmap);}
    auto selected=current_cursor&&cursor_count>=0?current_cursor:blank_cursor;for(auto& item:windows)XDefineCursor(display,item.second.xid,selected);XFlush(display);
}
inline GE6_WIN_MS uintptr_t set_cursor(uintptr_t cursor){trace("SetCursor");std::lock_guard<std::recursive_mutex> lock(mutex);if(!connect())return 0;if(cursor&&std::none_of(cursors.begin(),cursors.end(),[cursor](const auto& item){return item.second==cursor;})){set_error(6);return 0;}auto previous=current_cursor;current_cursor=cursor;apply_cursor();return previous;}
inline GE6_WIN_MS int show_cursor(int visible){trace("ShowCursor");std::lock_guard<std::recursive_mutex> lock(mutex);if(!connect())return cursor_count;cursor_count+=visible?1:-1;apply_cursor();return cursor_count;}
inline uint32_t milliseconds(){timespec now{};clock_gettime(CLOCK_MONOTONIC,&now);return uint32_t(uint64_t(now.tv_sec)*1000+uint64_t(now.tv_nsec)/1000000);}
inline WindowData* window(uintptr_t handle){auto found=windows.find(handle);if(found==windows.end()){set_error(1400);return nullptr;}return &found->second;}
inline GE6_WIN_MS int adjust(Rect* rect,uint32_t,int menu,uint32_t){trace("AdjustWindowRectEx");if(!rect||menu){set_error(87);return 0;}return 1;} // X11 client geometry; decorations belong to the host WM.
inline GE6_WIN_MS intptr_t default_proc(uintptr_t handle,uint32_t message,uintptr_t wp,intptr_t lp){
    trace("DefWindowProcW");if(message==0x81)return 1;
    if(message==0x80||message==0x7f){std::lock_guard<std::recursive_mutex> lock(mutex);auto* data=window(handle);if(!data)return 0;auto key=unsigned(wp);auto previous=data->icons.find(key);uintptr_t result=previous==data->icons.end()?0:previous->second.first;if(message==0x7f)return intptr_t(result);if(wp>1){set_error(87);return 0;}
        auto icon=lp?ge6_images::get(uintptr_t(lp)):nullptr;if(lp&&!icon){set_error(6);return 0;}if(icon)data->icons[key]={uintptr_t(lp),icon};else data->icons.erase(key);
        std::vector<unsigned long> payload;for(auto& item:data->icons){auto& frame=*item.second.second;payload.push_back(frame.width);payload.push_back(frame.height);for(size_t i=0;i<frame.rgba.size();i+=4)payload.push_back((static_cast<unsigned long>(frame.rgba[i+3])<<24)|(static_cast<unsigned long>(frame.rgba[i])<<16)|(static_cast<unsigned long>(frame.rgba[i+1])<<8)|frame.rgba[i+2]);}
        auto atom=XInternAtom(display,"_NET_WM_ICON",False);if(payload.empty())XDeleteProperty(display,data->xid,atom);else XChangeProperty(display,data->xid,atom,XA_CARDINAL,32,PropModeReplace,reinterpret_cast<const unsigned char*>(payload.data()),int(payload.size()));XFlush(display);std::fprintf(stderr,"GE6_X11_ICON hwnd=0x%lx size=%u width=%u height=%u\n",handle,key,icon?icon->width:0,icon?icon->height:0);return intptr_t(result);
    }return 0;
}
inline GE6_WIN_MS intptr_t call_proc(ge6_classes::Procedure procedure,uintptr_t handle,uint32_t message,uintptr_t wp,intptr_t lp){trace("CallWindowProcW");if(!procedure){set_error(87);return 0;}return procedure(handle,message,wp,lp);}
struct Create {uintptr_t parameter,instance,menu,parent;int32_t height,width,y,x;uint32_t style,pad;const uint16_t* name;const uint16_t* klass;uint32_t extended,pad2;};
static_assert(sizeof(Create)==80);
inline GE6_WIN_MS uintptr_t create(uint32_t extended,const uint16_t* raw_class,const uint16_t* title,uint32_t style,int x,int y,int width,int height,uintptr_t parent,uintptr_t menu,uintptr_t instance,uintptr_t parameter){
    trace("CreateWindowExW");auto caller=reinterpret_cast<uintptr_t>(__builtin_return_address(0));
    if(menu||width<0||height<0||width>16384||height>16384){set_error(87);return 0;}
    std::lock_guard<std::recursive_mutex> lock(mutex);if(!connect())return 0;std::shared_ptr<ge6_classes::Class> klass;
    {std::lock_guard<std::mutex> guard(ge6_classes::mutex);klass=ge6_classes::find(instance,ge6_images::name(raw_class));if(!klass){set_error(1407);return 0;}++klass->windows;}
    bool message_only=parent==uintptr_t(-3);Window host_parent=DefaultRootWindow(display);if(parent&&!message_only){auto* data=window(parent);if(!data){--klass->windows;return 0;}host_parent=data->xid;}
    auto xid=XCreateSimpleWindow(display,host_parent,x,y,unsigned(std::max(1,width)),unsigned(std::max(1,height)),0,0,0);if(!xid){--klass->windows;set_error(8);return 0;}
    XSelectInput(display,xid,StructureNotifyMask|ExposureMask|FocusChangeMask|KeyPressMask|KeyReleaseMask|ButtonPressMask|ButtonReleaseMask|PointerMotionMask|LeaveWindowMask);
    auto title_text=wide(title);auto name=ge6_fs::utf8(reinterpret_cast<const uint16_t*>(title_text.data()),title_text.size());XStoreName(display,xid,name.c_str());
    auto protocol=XInternAtom(display,"WM_DELETE_WINDOW",False);XSetWMProtocols(display,xid,&protocol,1);
    auto handle=next++;windows.emplace(handle,WindowData{xid,klass,klass->value.procedure,style,extended,x,y,width,height,instance,parent,0,message_only,{},{},{},0,false});
    Create info{parameter,instance,menu,parent,height,width,y,x,style,0,title,raw_class,extended,0};
    auto proc=klass->value.procedure;
    if(!proc(handle,0x81,0,reinterpret_cast<intptr_t>(&info))||proc(handle,1,0,reinterpret_cast<intptr_t>(&info))==-1){XDestroyWindow(display,xid);windows.erase(handle);--klass->windows;return 0;}
    if((style&0x10000000)&&!message_only)XMapWindow(display,xid);
    XSync(display,False);
    std::fprintf(stderr,"GE6_X11_WINDOW_CREATE hwnd=0x%lx xid=0x%lx width=%d height=%d mapped=%d procedure=0x%lx caller=0x%lx\n",handle,xid,width,height,int((style&0x10000000)&&!message_only),reinterpret_cast<uintptr_t>(proc),caller);
    return handle;
}
inline std::u16string property(const uint16_t* name){auto raw=reinterpret_cast<uintptr_t>(name);return raw<=65535?u"#"+ge6_fs::utf16(std::to_string(raw)):wide(name);}
inline GE6_WIN_MS uintptr_t get_property(uintptr_t handle,const uint16_t* name){trace("GetPropW");std::lock_guard<std::recursive_mutex> lock(mutex);auto* data=window(handle);if(!data)return 0;auto found=data->properties.find(property(name));return found==data->properties.end()?0:found->second;}
inline GE6_WIN_MS int set_property(uintptr_t handle,const uint16_t* name,uintptr_t value){trace("SetPropW");std::lock_guard<std::recursive_mutex> lock(mutex);auto* data=window(handle);if(!data)return 0;data->properties[property(name)]=value;return 1;}
inline GE6_WIN_MS uintptr_t remove_property(uintptr_t handle,const uint16_t* name){trace("RemovePropW");std::lock_guard<std::recursive_mutex> lock(mutex);auto* data=window(handle);if(!data)return 0;auto found=data->properties.find(property(name));if(found==data->properties.end())return 0;auto value=found->second;data->properties.erase(found);return value;}
inline GE6_WIN_MS intptr_t get_long(uintptr_t handle,int index){trace("GetWindowLongPtrW");std::lock_guard<std::recursive_mutex> lock(mutex);auto* data=window(handle);if(!data)return 0;switch(index){case -4:return reinterpret_cast<intptr_t>(data->procedure);case -6:return intptr_t(data->instance);case -8:return intptr_t(data->parent);case -16:return data->style;case -20:return data->extended;case -21:return intptr_t(data->user);default:stop("USER32.dll","GetWindowLongPtrW",reinterpret_cast<uintptr_t>(__builtin_return_address(0)));return 0;}}
inline GE6_WIN_MS intptr_t set_long(uintptr_t handle,int index,intptr_t value){trace("SetWindowLongPtrW");std::lock_guard<std::recursive_mutex> lock(mutex);auto previous=get_long(handle,index);auto* data=window(handle);if(!data)return 0;switch(index){case -4:data->procedure=reinterpret_cast<ge6_classes::Procedure>(value);break;case -16:data->style=uint32_t(value);break;case -20:data->extended=uint32_t(value);break;case -21:data->user=uintptr_t(value);break;default:stop("USER32.dll","SetWindowLongPtrW",reinterpret_cast<uintptr_t>(__builtin_return_address(0)));}return previous;}
inline GE6_WIN_MS int32_t get_long32(uintptr_t handle,int index){return int32_t(get_long(handle,index));}
inline GE6_WIN_MS int32_t set_long32(uintptr_t handle,int index,int32_t value){return int32_t(set_long(handle,index,value));}
inline GE6_WIN_MS int client_rect(uintptr_t handle,Rect* rect){trace("GetClientRect");std::lock_guard<std::recursive_mutex> lock(mutex);auto* data=window(handle);if(!data||!rect)return 0;XWindowAttributes attributes{};if(!XGetWindowAttributes(display,data->xid,&attributes))return 0;*rect={0,0,attributes.width,attributes.height};return 1;}
inline GE6_WIN_MS int window_rect(uintptr_t handle,Rect* rect){trace("GetWindowRect");std::lock_guard<std::recursive_mutex> lock(mutex);auto* data=window(handle);if(!data||!rect)return 0;XWindowAttributes attributes{};if(!XGetWindowAttributes(display,data->xid,&attributes))return 0;Window child=0;int x=0,y=0;XTranslateCoordinates(display,data->xid,DefaultRootWindow(display),0,0,&x,&y,&child);*rect={x,y,x+attributes.width,y+attributes.height};return 1;}
inline GE6_WIN_MS int to_screen(uintptr_t handle,Point* point){trace("ClientToScreen");std::lock_guard<std::recursive_mutex> lock(mutex);auto* data=window(handle);if(!data||!point)return 0;Window child=0;int x=0,y=0;if(!XTranslateCoordinates(display,data->xid,DefaultRootWindow(display),point->x,point->y,&x,&y,&child))return 0;*point={x,y};return 1;}
inline GE6_WIN_MS uintptr_t get_dc(uintptr_t handle){trace("GetDC");std::lock_guard<std::recursive_mutex> lock(mutex);if(!connect())return 0;Window drawable=DefaultRootWindow(display);int width=DisplayWidth(display,0),height=DisplayHeight(display,0);if(handle){auto* data=window(handle);if(!data)return 0;drawable=data->xid;width=data->width;height=data->height;}auto gc=XCreateGC(display,drawable,0,nullptr);if(!gc)return 0;auto token=next++;dcs.emplace(token,DC{drawable,gc,width,height,DisplayWidthMM(display,0),DisplayHeightMM(display,0),handle});std::fprintf(stderr,"GE6_X11_DC window=0x%lx dc=0x%lx drawable=0x%lx\n",handle,token,drawable);return token;}
inline GE6_WIN_MS int release_dc(uintptr_t handle,uintptr_t dc){trace("ReleaseDC");std::lock_guard<std::recursive_mutex> lock(mutex);auto found=dcs.find(dc);if(found==dcs.end()||found->second.window!=handle)return 0;return delete_dc(dc);}
inline intptr_t mouse_position(int x,int y){return intptr_t(int32_t(uint32_t(uint16_t(x))|(uint32_t(uint16_t(y))<<16)));}
inline uint32_t mouse_keys(unsigned state,uint32_t extras){return ((state&Button1Mask)?1u:0u)|((state&Button3Mask)?2u:0u)|((state&ShiftMask)?4u:0u)|((state&ControlMask)?8u:0u)|((state&Button2Mask)?16u:0u)|(extras&0x60u);}
inline uint32_t mouse_event_count=0;
inline bool mouse_message(uintptr_t handle,WindowData& data,const XEvent& event,Message& result){
    if(event.type!=MotionNotify&&event.type!=ButtonPress&&event.type!=ButtonRelease)return false;
    result={};result.window=handle;
    if(event.type==MotionNotify){result.message=0x200;result.wparam=mouse_keys(event.xmotion.state,data.mouse_buttons);result.lparam=mouse_position(event.xmotion.x,event.xmotion.y);result.point={event.xmotion.x_root,event.xmotion.y_root};result.time=uint32_t(event.xmotion.time);}
    else{
        const auto& button=event.xbutton;bool pressed=event.type==ButtonPress;
        auto keys=mouse_keys(button.state,data.mouse_buttons);result.point={button.x_root,button.y_root};result.time=uint32_t(button.time);
        if(button.button>=4&&button.button<=7){if(!pressed)return false;bool horizontal=button.button>=6;int delta=(button.button==4||button.button==7)?120:-120;result.message=horizontal?0x20e:0x20a;result.wparam=keys|(uintptr_t(uint16_t(delta))<<16);result.lparam=mouse_position(button.x_root,button.y_root);}
        else{
            uint32_t bit=0;switch(button.button){case 1:bit=1;result.message=pressed?0x201:0x202;break;case 2:bit=16;result.message=pressed?0x207:0x208;break;case 3:bit=2;result.message=pressed?0x204:0x205;break;case 8:case 9:bit=button.button==8?32:64;result.message=pressed?0x20b:0x20c;break;default:return false;}
            keys=pressed?(keys|bit):(keys&~bit);result.wparam=keys;if(button.button>=8)result.wparam|=uintptr_t(button.button==8?1:2)<<16;
            result.lparam=mouse_position(button.x,button.y);
        }
        data.mouse_buttons=uint32_t(result.wparam)&0x73u;
    }
    if(++mouse_event_count<=64)std::fprintf(stderr,"GE6_X11_MOUSE_EVENT hwnd=0x%lx message=0x%x keys=0x%lx root_x=%d root_y=%d time=%u\n",handle,result.message,result.wparam,result.point.x,result.point.y,result.time);
    return true;
}
inline uintptr_t handle_for_xid(Window xid){for(auto& item:windows)if(item.second.xid==xid)return item.first;return 0;}
inline void focus_event(uintptr_t handle,const XFocusChangeEvent& event){
    // Grabs and focus moving into a child do not deactivate the top-level window.
    if(event.mode!=NotifyNormal||event.detail==NotifyInferior)return;
    const bool active=event.type==FocusIn;
    if((active&&foreground==handle)||(!active&&foreground!=handle))return;
    Window focused=0;int revert=0;XGetInputFocus(display,&focused,&revert);
    auto other=active?foreground:handle_for_xid(focused);
    foreground=active?handle:0;
    messages.push_back(Message{handle,6,0,uintptr_t(active),intptr_t(other),milliseconds(),{0,0},0});
    messages.push_back(Message{handle,active?7u:8u,0,other,0,milliseconds(),{0,0},0});
    std::fprintf(stderr,"GE6_X11_FOCUS hwnd=0x%lx active=%d other=0x%lx mode=%d detail=%d\n",handle,int(active),other,event.mode,event.detail);
}
inline void events(){
    if(!display)return;
    while(XPending(display)){XEvent event{};XNextEvent(display,&event);uintptr_t handle=0;for(auto& item:windows)if(item.second.xid==event.xany.window){handle=item.first;break;}if(!handle)continue;
        auto& data=windows.at(handle);uint32_t type=0;uintptr_t wp=0;intptr_t lp=0;
        if(event.type==FocusIn||event.type==FocusOut){focus_event(handle,event.xfocus);continue;}
        Message mouse{};if(mouse_message(handle,data,event,mouse)){messages.push_back(mouse);continue;}
        if(event.type==LeaveNotify&&event.xcrossing.detail!=NotifyInferior&&data.mouse_leave){data.mouse_leave=false;messages.push_back(Message{handle,0x2a3,0,0,0,uint32_t(event.xcrossing.time),{event.xcrossing.x_root,event.xcrossing.y_root},0});continue;}
        switch(event.type){case Expose:data.damaged.push_back({event.xexpose.x,event.xexpose.y,event.xexpose.x+event.xexpose.width,event.xexpose.y+event.xexpose.height});type=15;break;case ConfigureNotify:data.x=event.xconfigure.x;data.y=event.xconfigure.y;data.width=event.xconfigure.width;data.height=event.xconfigure.height;type=5;lp=intptr_t((uint32_t(data.height)<<16)|(uint32_t(data.width)&65535));break;case MapNotify:data.style|=0x10000000;type=24;wp=1;break;case UnmapNotify:data.style&=~0x10000000u;type=24;break;case ClientMessage:if(Atom(event.xclient.data.l[0])==XInternAtom(display,"WM_DELETE_WINDOW",False))type=16;break;default:break;}
        if(type)messages.push_back(Message{handle,type,0,wp,lp,milliseconds(),{0,0},0});
    }
}
struct MouseTracking {uint32_t size,flags;uintptr_t window;uint32_t hover_time,pad;};
static_assert(sizeof(MouseTracking)==24&&offsetof(MouseTracking,window)==8&&offsetof(MouseTracking,hover_time)==16);
inline bool pointer_inside(const WindowData& owner){
    XWindowAttributes attributes{};if(!XGetWindowAttributes(display,owner.xid,&attributes)||attributes.map_state!=IsViewable)return false;
    Window target=DefaultRootWindow(display);for(unsigned depth=0;target&&depth<128;++depth){Window root=0,child=0;int root_x=0,root_y=0,x=0,y=0;unsigned state=0;if(!XQueryPointer(display,target,&root,&child,&root_x,&root_y,&x,&y,&state))return false;if(target==owner.xid)return x>=0&&y>=0&&x<attributes.width&&y<attributes.height;target=child;}return false;
}
inline GE6_WIN_MS int track_mouse(MouseTracking* request){
    trace("TrackMouseEvent");std::lock_guard<std::recursive_mutex> lock(mutex);
    if(!request||request->size!=24){set_error(87);return 0;}if(request->flags!=2&&request->flags!=0x80000002u){set_error(50);return 0;}
    auto* data=window(request->window);if(!data)return 0;events();
    if(request->flags&0x80000000u){data->mouse_leave=false;return 1;}
    data->mouse_leave=pointer_inside(*data);if(!data->mouse_leave){Point point{};Window root=0,child=0;int x=0,y=0;unsigned state=0;XQueryPointer(display,DefaultRootWindow(display),&root,&child,&point.x,&point.y,&x,&y,&state);messages.push_back(Message{request->window,0x2a3,0,0,0,milliseconds(),point,0});}return 1;
}
struct RawInputDevice {uint16_t page,usage;uint32_t flags;uintptr_t target;};
static_assert(sizeof(RawInputDevice)==16&&offsetof(RawInputDevice,target)==8);
inline GE6_WIN_MS int register_raw_input(const RawInputDevice* request,uint32_t count,uint32_t size){
    trace("RegisterRawInputDevices");if(!request||!count||size!=16){set_error(87);return 0;}if(count!=1||request->page!=1||request->usage!=2){set_error(50);return 0;}
    if((request->flags&1)&&request->target){set_error(87);return 0;}
    // No raw-input subscription is ever created by this bounded backend.
    // Removing an absent mouse collection is idempotent, verified on Windows.
    if(request->flags==1){std::fprintf(stderr,"GE6_X11_RAW_INPUT_REMOVE empty=1 caller=0x%lx\n",reinterpret_cast<uintptr_t>(__builtin_return_address(0)));return 1;}
    set_error(50);return 0; // Registration/delivery of Windows raw input is unavailable.
}
inline GE6_WIN_MS int clip_cursor(const Rect* requested){
    trace("ClipCursor");std::lock_guard<std::recursive_mutex> lock(mutex);if(!connect())return 0;
    // This backend installs no confinement. Releasing that empty state succeeds;
    // never release another client's X grab or pretend a requested clip exists.
    if(requested){set_error(50);return 0;}return 1;
}
inline GE6_WIN_MS int update_rect(uintptr_t handle,Rect* result,int erase){
    trace("GetUpdateRect");std::lock_guard<std::recursive_mutex> lock(mutex);auto* data=window(handle);if(!data)return 0;events();
    if(data->damaged.empty()){if(result)*result={0,0,0,0};return 0;}
    if(erase){stop("USER32.dll","GetUpdateRect(background-erase)",reinterpret_cast<uintptr_t>(__builtin_return_address(0)));return 0;}
    auto bounds=data->damaged.front();for(auto& part:data->damaged){bounds.left=std::min(bounds.left,part.left);bounds.top=std::min(bounds.top,part.top);bounds.right=std::max(bounds.right,part.right);bounds.bottom=std::max(bounds.bottom,part.bottom);}if(result)*result=bounds;return 1;
}
inline GE6_WIN_MS int validate_rect(uintptr_t handle,const Rect* validated){
    trace("ValidateRect");std::lock_guard<std::recursive_mutex> lock(mutex);auto* data=window(handle);if(!data)return 0;events();
    if(!validated)data->damaged.clear();else{std::vector<Rect> remaining;for(auto& part:data->damaged){Rect hit{std::max(part.left,validated->left),std::max(part.top,validated->top),std::min(part.right,validated->right),std::min(part.bottom,validated->bottom)};if(hit.left>=hit.right||hit.top>=hit.bottom){remaining.push_back(part);continue;}for(auto piece:{Rect{part.left,part.top,part.right,hit.top},Rect{part.left,hit.bottom,part.right,part.bottom},Rect{part.left,hit.top,hit.left,hit.bottom},Rect{hit.right,hit.top,part.right,hit.bottom}})if(piece.left<piece.right&&piece.top<piece.bottom)remaining.push_back(piece);}data->damaged=std::move(remaining);}
    if(data->damaged.empty()){messages.erase(std::remove_if(messages.begin(),messages.end(),[handle](const Message& item){return item.window==handle&&item.message==15;}),messages.end());}
    return 1;
}
inline GE6_WIN_MS int peek(Message* result,uintptr_t filter,uint32_t first,uint32_t last,uint32_t flags){trace("PeekMessageW");std::lock_guard<std::recursive_mutex> lock(mutex);if(!result||flags>1){set_error(87);return 0;}events();for(auto it=messages.begin();it!=messages.end();++it)if((!filter||filter==it->window)&&((!first&&!last)||(it->message>=first&&it->message<=last))){*result=*it;if(flags&1)messages.erase(it);return 1;}return 0;}
inline GE6_WIN_MS int translate(const Message*){trace("TranslateMessage");return 0;} // Character translation is added with keyboard event delivery, not synthesized here.
inline GE6_WIN_MS intptr_t dispatch(const Message* message){trace("DispatchMessageW");std::lock_guard<std::recursive_mutex> lock(mutex);if(!message)return 0;auto* data=window(message->window);return data?data->procedure(message->window,message->message,message->wparam,message->lparam):0;}
inline GE6_WIN_MS int16_t key_state(int code){trace("GetKeyState");std::lock_guard<std::recursive_mutex> lock(mutex);if(!connect())return 0;char keys[32]{};XQueryKeymap(display,keys);std::vector<int> scans;if(code==0xa0)scans={42};else if(code==0xa1)scans={54};else for(size_t i=0;i<scan_vk.size();++i)if(scan_vk[i]==code)scans.push_back(int(i));for(auto scan:scans){auto key=scan+8;if(keys[key/8]&(1<<(key%8)))return int16_t(0x8000);}return 0;}
inline std::array<bool,256> async_down{};
inline GE6_WIN_MS int16_t async_key_state(int code){
    trace("GetAsyncKeyState");std::lock_guard<std::recursive_mutex> lock(mutex);
    if(code<1||code>255){set_error(87);return 0;}if(!connect())return 0;
    bool down=false;
    if(code==1||code==2||code==4){
        Window root=0,child=0;int rx=0,ry=0,x=0,y=0;unsigned state=0;
        if(!XQueryPointer(display,DefaultRootWindow(display),&root,&child,&rx,&ry,&x,&y,&state))return 0;
        // X state contains logical buttons; Windows asks for physical buttons.
        unsigned char mapping[256]{};int count=XGetPointerMapping(display,mapping,256);
        unsigned physical=code==1?1u:(code==2?3u:2u);
        unsigned logical=count>=int(physical)?mapping[physical-1]:0;
        if(logical>5){set_error(50);return 0;}
        down=logical&&(state&(Button1Mask<<(logical-1)));
    }else if(code==5||code==6){set_error(50);return 0;} // Core X11 cannot query held buttons 8/9.
    else down=bool(uint16_t(key_state(code))&0x8000u);
    bool pressed=down&&!async_down[size_t(code)];async_down[size_t(code)]=down;
    // Only observed press transitions set bit 0; transient unobserved presses
    // may be missed. Original SDL activation reads only the current-state bit.
    return int16_t((down?0x8000u:0u)|(pressed?1u:0u));
}
inline GE6_WIN_MS uint32_t clipboard_sequence_unavailable(){trace("GetClipboardSequenceNumber");set_error(50);return 0;}
inline GE6_WIN_MS uintptr_t focus(){trace("GetFocus");std::lock_guard<std::recursive_mutex> lock(mutex);if(!connect())return 0;Window xid=0;int revert=0;XGetInputFocus(display,&xid,&revert);for(auto& item:windows)if(item.second.xid==xid)return item.first;return 0;}
inline GE6_WIN_MS uintptr_t menu(uintptr_t handle){trace("GetMenu");std::lock_guard<std::recursive_mutex> lock(mutex);if(!window(handle))return 0;return 0;} // CreateWindowExW accepts only menu-less windows.
inline GE6_WIN_MS int metrics(int index){trace("GetSystemMetrics");std::lock_guard<std::recursive_mutex> lock(mutex);if(!refresh())return 0;int left=outputs[0].x,top=outputs[0].y,right=left+outputs[0].width,bottom=top+outputs[0].height;for(auto& item:outputs){left=std::min(left,item.x);top=std::min(top,item.y);right=std::max(right,item.x+item.width);bottom=std::max(bottom,item.y+item.height);}switch(index){case 0:return DisplayWidth(display,0);case 1:return DisplayHeight(display,0);case 19:return 1;case 43:{unsigned char map[256]{};return XGetPointerMapping(display,map,256);}case 76:return left;case 77:return top;case 78:return right-left;case 79:return bottom-top;case 80:return int(outputs.size());default:std::fprintf(stderr,"GE6_X11_UNSUPPORTED_METRIC index=%d\n",index);stop("USER32.dll","GetSystemMetrics",reinterpret_cast<uintptr_t>(__builtin_return_address(0)));return 0;}}
inline GE6_WIN_MS int cursor_position(Point* point){trace("GetCursorPos");std::lock_guard<std::recursive_mutex> lock(mutex);if(!point||!connect())return 0;Window root=0,child=0;int x=0,y=0,wx=0,wy=0;unsigned mask=0;if(!XQueryPointer(display,DefaultRootWindow(display),&root,&child,&x,&y,&wx,&wy,&mask))return 0;*point={x,y};return 1;}
inline GE6_WIN_MS int to_client(uintptr_t handle,Point* point){trace("ScreenToClient");std::lock_guard<std::recursive_mutex> lock(mutex);auto* data=window(handle);if(!data||!point)return 0;Window child=0;int x=0,y=0;if(!XTranslateCoordinates(display,DefaultRootWindow(display),data->xid,point->x,point->y,&x,&y,&child))return 0;*point={x,y};return 1;}
inline GE6_WIN_MS int warp_cursor(int x,int y){trace("SetCursorPos");std::lock_guard<std::recursive_mutex> lock(mutex);if(!connect())return 0;XWarpPointer(display,0,DefaultRootWindow(display),0,0,0,0,x,y);XFlush(display);return 1;}
inline GE6_WIN_MS int destroy(uintptr_t handle){trace("DestroyWindow");std::lock_guard<std::recursive_mutex> lock(mutex);auto* data=window(handle);if(!data)return 0;data->procedure(handle,2,0,0);data->procedure(handle,0x82,0,0);auto klass=data->klass;XDestroyWindow(display,data->xid);windows.erase(handle);if(foreground==handle)foreground=0;{std::lock_guard<std::mutex> guard(ge6_classes::mutex);--klass->windows;}XSync(display,False);std::fprintf(stderr,"GE6_X11_WINDOW_DESTROY hwnd=0x%lx\n",handle);return 1;}
inline GE6_WIN_MS int show(uintptr_t handle,int how){trace("ShowWindow");std::lock_guard<std::recursive_mutex> lock(mutex);auto* data=window(handle);if(!data)return 0;bool previous=data->style&0x10000000;if(how==0){XUnmapWindow(display,data->xid);data->style&=~0x10000000u;}else if(how==1||how==4||how==5||how==8||how==9||how==10){if(!data->message_only){XMapWindow(display,data->xid);data->style|=0x10000000;}}else {stop("USER32.dll","ShowWindow",reinterpret_cast<uintptr_t>(__builtin_return_address(0)));return 0;}XSync(display,False);if(how!=0&&how!=4&&how!=8&&!data->message_only&&!data->parent&&!(data->extended&0x08000000u)){XSetInputFocus(display,data->xid,RevertToPointerRoot,CurrentTime);XSync(display,False);}std::fprintf(stderr,"GE6_X11_WINDOW_SHOW hwnd=0x%lx visible=%d caller=0x%lx\n",handle,int(bool(data->style&0x10000000)),reinterpret_cast<uintptr_t>(__builtin_return_address(0)));return previous;}
inline GE6_WIN_MS int title(uintptr_t handle,const uint16_t* value){trace("SetWindowTextW");std::lock_guard<std::recursive_mutex> lock(mutex);auto* data=window(handle);if(!data)return 0;auto text=wide(value);auto bytes=ge6_fs::utf8(reinterpret_cast<const uint16_t*>(text.data()),text.size());XStoreName(display,data->xid,bytes.c_str());XFlush(display);return 1;}
inline GE6_WIN_MS int position(uintptr_t handle,uintptr_t after,int x,int y,int width,int height,uint32_t flags){trace("SetWindowPos");std::lock_guard<std::recursive_mutex> lock(mutex);auto* data=window(handle);if(!data)return 0;if((flags&~uint32_t(0x77f))||(after!=0&&after!=1&&after!=uintptr_t(-1)&&after!=uintptr_t(-2))){stop("USER32.dll","SetWindowPos",reinterpret_cast<uintptr_t>(__builtin_return_address(0)));return 0;}XWindowChanges change{};unsigned mask=0;if(!(flags&2)){change.x=x;change.y=y;mask|=CWX|CWY;}if(!(flags&1)){if(width<=0||height<=0){set_error(87);return 0;}change.width=width;change.height=height;mask|=CWWidth|CWHeight;}if(!(flags&4)){change.stack_mode=after==1?Below:Above;mask|=CWStackMode;}if(mask)XConfigureWindow(display,data->xid,mask,&change);if(flags&0x40)show(handle,5);if(flags&0x80)show(handle,0);XSync(display,False);return 1;}
inline GE6_WIN_MS intptr_t send(uintptr_t handle,uint32_t message,uintptr_t wp,intptr_t lp){trace("SendMessageW");std::lock_guard<std::recursive_mutex> lock(mutex);auto* data=window(handle);return data?data->procedure(handle,message,wp,lp):0;}
inline GE6_WIN_MS intptr_t send_a(uintptr_t handle,uint32_t message,uintptr_t wp,intptr_t lp){trace("SendMessageA");if(message!=0x80&&message!=0x7f){stop("USER32.dll","SendMessageA",reinterpret_cast<uintptr_t>(__builtin_return_address(0)));return 0;}return send(handle,message,wp,lp);}
inline GE6_WIN_MS uintptr_t load_icon_a(uintptr_t instance,const char* name){trace("LoadIconA");if(reinterpret_cast<uintptr_t>(name)>65535){stop("USER32.dll","LoadIconA",reinterpret_cast<uintptr_t>(__builtin_return_address(0)));return 0;}return ge6_images::load(instance,reinterpret_cast<const uint16_t*>(name),1,0,0,0x8040);}
inline GE6_WIN_MS void drag_files(uintptr_t handle,int enabled){trace("DragAcceptFiles");if(!window(handle))return;if(enabled)std::fprintf(stderr,"GE6_X11_FILE_DROP unavailable=1 hwnd=0x%lx\n",handle);} // Shell advisory API is void; no file drop support claimed.
inline bool capture_on_stop(){
    const char* path=std::getenv("GE6_X11_CAPTURE");if(!path||!*path||!display)return false;
    std::lock_guard<std::recursive_mutex> lock(mutex);XSync(display,False);
    for(auto& item:windows){XWindowAttributes attributes{};if(XGetWindowAttributes(display,item.second.xid,&attributes))std::fprintf(stderr,"GE6_X11_WINDOW_SNAPSHOT hwnd=0x%lx xid=0x%lx width=%d height=%d viewable=%d procedure=0x%lx\n",item.first,item.second.xid,attributes.width,attributes.height,int(attributes.map_state==IsViewable),reinterpret_cast<uintptr_t>(item.second.procedure));}
    int width=DisplayWidth(display,0),height=DisplayHeight(display,0);if(width<=0||height<=0||width>8192||height>8192)return false;
    auto* source=XGetImage(display,DefaultRootWindow(display),0,0,unsigned(width),unsigned(height),AllPlanes,ZPixmap);if(!source)return false;
    std::vector<uint8_t> pixels(size_t(width)*size_t(height)*4);
    auto channel=[](unsigned long pixel,unsigned long mask){if(!mask)return uint8_t(0);while(!(mask&1)){mask>>=1;pixel>>=1;}return uint8_t(((pixel&mask)*255+mask/2)/mask);};
    for(int y=0;y<height;++y)for(int x=0;x<width;++x){auto value=XGetPixel(source,x,y);auto at=(size_t(y)*size_t(width)+size_t(x))*4;pixels[at]=channel(value,source->red_mask);pixels[at+1]=channel(value,source->green_mask);pixels[at+2]=channel(value,source->blue_mask);pixels[at+3]=255;}
    XDestroyImage(source);png_image image{};image.version=PNG_IMAGE_VERSION;image.width=unsigned(width);image.height=unsigned(height);image.format=PNG_FORMAT_RGBA;
    bool written=png_image_write_to_file(&image,path,0,pixels.data(),0,nullptr);if(written)std::fprintf(stderr,"GE6_X11_CAPTURE_SAVED width=%d height=%d path=%s\n",width,height,path);
    png_image_free(&image);return written;
}
inline uintptr_t resolve(const std::string& library,const std::string& name){
    if(library=="SHELL32.dll"&&name=="DragAcceptFiles")return reinterpret_cast<uintptr_t>(&drag_files);
    if(library!="USER32.dll")return 0;
#define WINDOW_BIND(n,f) if(name==n)return reinterpret_cast<uintptr_t>(&f)
    WINDOW_BIND("MapVirtualKeyW",map_key);WINDOW_BIND("LoadCursorW",load_cursor);WINDOW_BIND("GetDoubleClickTime",double_click);
    WINDOW_BIND("AdjustWindowRectEx",adjust);WINDOW_BIND("CreateWindowExW",create);WINDOW_BIND("DefWindowProcW",default_proc);
    WINDOW_BIND("CallWindowProcW",call_proc);WINDOW_BIND("GetSystemMetrics",metrics);WINDOW_BIND("GetCursorPos",cursor_position);WINDOW_BIND("ScreenToClient",to_client);WINDOW_BIND("SetCursorPos",warp_cursor);WINDOW_BIND("GetForegroundWindow",focus);
    WINDOW_BIND("GetMenu",menu);
    WINDOW_BIND("GetUpdateRect",update_rect);WINDOW_BIND("ValidateRect",validate_rect);
    WINDOW_BIND("TrackMouseEvent",track_mouse);
    WINDOW_BIND("RegisterRawInputDevices",register_raw_input);WINDOW_BIND("ClipCursor",clip_cursor);
    WINDOW_BIND("IsRectEmpty",rect_empty);
    WINDOW_BIND("SetCursor",set_cursor);WINDOW_BIND("ShowCursor",show_cursor);
    WINDOW_BIND("LoadIconA",load_icon_a);WINDOW_BIND("SendMessageA",send_a);
    WINDOW_BIND("GetPropW",get_property);WINDOW_BIND("SetPropW",set_property);WINDOW_BIND("RemovePropW",remove_property);
    WINDOW_BIND("GetWindowLongPtrW",get_long);WINDOW_BIND("SetWindowLongPtrW",set_long);WINDOW_BIND("GetWindowLongW",get_long32);WINDOW_BIND("SetWindowLongW",set_long32);
    WINDOW_BIND("GetClientRect",client_rect);WINDOW_BIND("GetWindowRect",window_rect);WINDOW_BIND("ClientToScreen",to_screen);WINDOW_BIND("GetDC",get_dc);WINDOW_BIND("ReleaseDC",release_dc);
    WINDOW_BIND("PeekMessageW",peek);WINDOW_BIND("TranslateMessage",translate);WINDOW_BIND("DispatchMessageW",dispatch);WINDOW_BIND("GetKeyState",key_state);WINDOW_BIND("GetAsyncKeyState",async_key_state);WINDOW_BIND("GetClipboardSequenceNumber",clipboard_sequence_unavailable);WINDOW_BIND("GetFocus",focus);
    WINDOW_BIND("DestroyWindow",destroy);WINDOW_BIND("ShowWindow",show);WINDOW_BIND("SetWindowTextW",title);WINDOW_BIND("SetWindowPos",position);WINDOW_BIND("SendMessageW",send);
#undef WINDOW_BIND
    return 0;
}
#undef GE6_WIN_MS
}
#endif
