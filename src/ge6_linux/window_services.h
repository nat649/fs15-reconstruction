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
struct Point {int32_t x,y;};
struct Message {uintptr_t window;uint32_t message,pad;uintptr_t wparam;intptr_t lparam;uint32_t time;Point point;uint32_t pad2;};
static_assert(sizeof(Message)==48);
struct WindowData {Window xid;std::shared_ptr<ge6_classes::Class> klass;ge6_classes::Procedure procedure;uint32_t style,extended;int x,y,width,height;uintptr_t instance,parent,user=0;bool message_only=false;std::map<std::u16string,uintptr_t> properties;std::map<unsigned,std::pair<uintptr_t,std::shared_ptr<ge6_images::Icon>>> icons;};
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
    XSelectInput(display,xid,StructureNotifyMask|ExposureMask|FocusChangeMask|KeyPressMask|KeyReleaseMask|ButtonPressMask|ButtonReleaseMask|PointerMotionMask);
    auto title_text=wide(title);auto name=ge6_fs::utf8(reinterpret_cast<const uint16_t*>(title_text.data()),title_text.size());XStoreName(display,xid,name.c_str());
    auto protocol=XInternAtom(display,"WM_DELETE_WINDOW",False);XSetWMProtocols(display,xid,&protocol,1);
    auto handle=next++;windows.emplace(handle,WindowData{xid,klass,klass->value.procedure,style,extended,x,y,width,height,instance,parent,0,message_only,{},{}});
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
inline void events(){
    if(!display)return;
    while(XPending(display)){XEvent event{};XNextEvent(display,&event);uintptr_t handle=0;for(auto& item:windows)if(item.second.xid==event.xany.window){handle=item.first;break;}if(!handle)continue;
        auto& data=windows.at(handle);uint32_t type=0;uintptr_t wp=0;intptr_t lp=0;
        switch(event.type){case Expose:type=15;break;case ConfigureNotify:data.x=event.xconfigure.x;data.y=event.xconfigure.y;data.width=event.xconfigure.width;data.height=event.xconfigure.height;type=5;lp=intptr_t((uint32_t(data.height)<<16)|(uint32_t(data.width)&65535));break;case MapNotify:data.style|=0x10000000;type=24;wp=1;break;case UnmapNotify:data.style&=~0x10000000u;type=24;break;case FocusIn:foreground=handle;type=7;break;case FocusOut:if(foreground==handle)foreground=0;type=8;break;case ClientMessage:if(Atom(event.xclient.data.l[0])==XInternAtom(display,"WM_DELETE_WINDOW",False))type=16;break;default:break;}
        if(type)messages.push_back(Message{handle,type,0,wp,lp,milliseconds(),{0,0},0});
    }
}
inline GE6_WIN_MS int peek(Message* result,uintptr_t filter,uint32_t first,uint32_t last,uint32_t flags){trace("PeekMessageW");std::lock_guard<std::recursive_mutex> lock(mutex);if(!result||flags>1){set_error(87);return 0;}events();for(auto it=messages.begin();it!=messages.end();++it)if((!filter||filter==it->window)&&((!first&&!last)||(it->message>=first&&it->message<=last))){*result=*it;if(flags&1)messages.erase(it);return 1;}return 0;}
inline GE6_WIN_MS int translate(const Message*){trace("TranslateMessage");return 0;} // Character translation is added with keyboard event delivery, not synthesized here.
inline GE6_WIN_MS intptr_t dispatch(const Message* message){trace("DispatchMessageW");std::lock_guard<std::recursive_mutex> lock(mutex);if(!message)return 0;auto* data=window(message->window);return data?data->procedure(message->window,message->message,message->wparam,message->lparam):0;}
inline GE6_WIN_MS int16_t key_state(int code){trace("GetKeyState");std::lock_guard<std::recursive_mutex> lock(mutex);if(!connect())return 0;char keys[32]{};XQueryKeymap(display,keys);std::vector<int> scans;if(code==0xa0)scans={42};else if(code==0xa1)scans={54};else for(size_t i=0;i<scan_vk.size();++i)if(scan_vk[i]==code)scans.push_back(int(i));for(auto scan:scans){auto key=scan+8;if(keys[key/8]&(1<<(key%8)))return int16_t(0x8000);}return 0;}
inline GE6_WIN_MS uintptr_t focus(){trace("GetFocus");std::lock_guard<std::recursive_mutex> lock(mutex);if(!connect())return 0;Window xid=0;int revert=0;XGetInputFocus(display,&xid,&revert);for(auto& item:windows)if(item.second.xid==xid)return item.first;return 0;}
inline GE6_WIN_MS uintptr_t menu(uintptr_t handle){trace("GetMenu");std::lock_guard<std::recursive_mutex> lock(mutex);if(!window(handle))return 0;return 0;} // CreateWindowExW accepts only menu-less windows.
inline GE6_WIN_MS int metrics(int index){trace("GetSystemMetrics");std::lock_guard<std::recursive_mutex> lock(mutex);if(!refresh())return 0;int left=outputs[0].x,top=outputs[0].y,right=left+outputs[0].width,bottom=top+outputs[0].height;for(auto& item:outputs){left=std::min(left,item.x);top=std::min(top,item.y);right=std::max(right,item.x+item.width);bottom=std::max(bottom,item.y+item.height);}switch(index){case 0:return DisplayWidth(display,0);case 1:return DisplayHeight(display,0);case 19:return 1;case 43:{unsigned char map[256]{};return XGetPointerMapping(display,map,256);}case 76:return left;case 77:return top;case 78:return right-left;case 79:return bottom-top;case 80:return int(outputs.size());default:std::fprintf(stderr,"GE6_X11_UNSUPPORTED_METRIC index=%d\n",index);stop("USER32.dll","GetSystemMetrics",reinterpret_cast<uintptr_t>(__builtin_return_address(0)));return 0;}}
inline GE6_WIN_MS int cursor_position(Point* point){trace("GetCursorPos");std::lock_guard<std::recursive_mutex> lock(mutex);if(!point||!connect())return 0;Window root=0,child=0;int x=0,y=0,wx=0,wy=0;unsigned mask=0;if(!XQueryPointer(display,DefaultRootWindow(display),&root,&child,&x,&y,&wx,&wy,&mask))return 0;*point={x,y};return 1;}
inline GE6_WIN_MS int to_client(uintptr_t handle,Point* point){trace("ScreenToClient");std::lock_guard<std::recursive_mutex> lock(mutex);auto* data=window(handle);if(!data||!point)return 0;Window child=0;int x=0,y=0;if(!XTranslateCoordinates(display,DefaultRootWindow(display),data->xid,point->x,point->y,&x,&y,&child))return 0;*point={x,y};return 1;}
inline GE6_WIN_MS int warp_cursor(int x,int y){trace("SetCursorPos");std::lock_guard<std::recursive_mutex> lock(mutex);if(!connect())return 0;XWarpPointer(display,0,DefaultRootWindow(display),0,0,0,0,x,y);XFlush(display);return 1;}
inline GE6_WIN_MS int destroy(uintptr_t handle){trace("DestroyWindow");std::lock_guard<std::recursive_mutex> lock(mutex);auto* data=window(handle);if(!data)return 0;data->procedure(handle,2,0,0);data->procedure(handle,0x82,0,0);auto klass=data->klass;XDestroyWindow(display,data->xid);windows.erase(handle);{std::lock_guard<std::mutex> guard(ge6_classes::mutex);--klass->windows;}XSync(display,False);std::fprintf(stderr,"GE6_X11_WINDOW_DESTROY hwnd=0x%lx\n",handle);return 1;}
inline GE6_WIN_MS int show(uintptr_t handle,int how){trace("ShowWindow");std::lock_guard<std::recursive_mutex> lock(mutex);auto* data=window(handle);if(!data)return 0;bool previous=data->style&0x10000000;if(how==0){XUnmapWindow(display,data->xid);data->style&=~0x10000000u;}else if(how==1||how==5||how==8||how==9||how==10){if(!data->message_only){XMapWindow(display,data->xid);data->style|=0x10000000;}}else {stop("USER32.dll","ShowWindow",reinterpret_cast<uintptr_t>(__builtin_return_address(0)));return 0;}XSync(display,False);std::fprintf(stderr,"GE6_X11_WINDOW_SHOW hwnd=0x%lx visible=%d caller=0x%lx\n",handle,int(bool(data->style&0x10000000)),reinterpret_cast<uintptr_t>(__builtin_return_address(0)));return previous;}
inline GE6_WIN_MS int title(uintptr_t handle,const uint16_t* value){trace("SetWindowTextW");std::lock_guard<std::recursive_mutex> lock(mutex);auto* data=window(handle);if(!data)return 0;auto text=wide(value);auto bytes=ge6_fs::utf8(reinterpret_cast<const uint16_t*>(text.data()),text.size());XStoreName(display,data->xid,bytes.c_str());XFlush(display);return 1;}
inline GE6_WIN_MS int position(uintptr_t handle,uintptr_t after,int x,int y,int width,int height,uint32_t flags){trace("SetWindowPos");std::lock_guard<std::recursive_mutex> lock(mutex);auto* data=window(handle);if(!data)return 0;if((flags&~uint32_t(0x77f))||(after!=0&&after!=1&&after!=uintptr_t(-1)&&after!=uintptr_t(-2))){stop("USER32.dll","SetWindowPos",reinterpret_cast<uintptr_t>(__builtin_return_address(0)));return 0;}XWindowChanges change{};unsigned mask=0;if(!(flags&2)){change.x=x;change.y=y;mask|=CWX|CWY;}if(!(flags&1)){if(width<=0||height<=0){set_error(87);return 0;}change.width=width;change.height=height;mask|=CWWidth|CWHeight;}if(!(flags&4)){change.stack_mode=after==1?Below:Above;mask|=CWStackMode;}if(mask)XConfigureWindow(display,data->xid,mask,&change);if(flags&0x40)show(handle,5);if(flags&0x80)show(handle,0);XSync(display,False);return 1;}
inline GE6_WIN_MS intptr_t send(uintptr_t handle,uint32_t message,uintptr_t wp,intptr_t lp){trace("SendMessageW");std::lock_guard<std::recursive_mutex> lock(mutex);auto* data=window(handle);return data?data->procedure(handle,message,wp,lp):0;}
inline GE6_WIN_MS intptr_t send_a(uintptr_t handle,uint32_t message,uintptr_t wp,intptr_t lp){trace("SendMessageA");if(message!=0x80&&message!=0x7f){stop("USER32.dll","SendMessageA",reinterpret_cast<uintptr_t>(__builtin_return_address(0)));return 0;}return send(handle,message,wp,lp);}
inline GE6_WIN_MS uintptr_t load_icon_a(uintptr_t instance,const char* name){trace("LoadIconA");if(reinterpret_cast<uintptr_t>(name)>65535){stop("USER32.dll","LoadIconA",reinterpret_cast<uintptr_t>(__builtin_return_address(0)));return 0;}return ge6_images::load(instance,reinterpret_cast<const uint16_t*>(name),1,0,0,0x8040);}
inline GE6_WIN_MS void drag_files(uintptr_t handle,int enabled){trace("DragAcceptFiles");if(!window(handle))return;if(enabled)std::fprintf(stderr,"GE6_X11_FILE_DROP unavailable=1 hwnd=0x%lx\n",handle);} // Shell advisory API is void; no file drop support claimed.
inline void capture_on_stop(){
    const char* path=std::getenv("GE6_X11_CAPTURE");if(!path||!*path||!display)return;
    std::lock_guard<std::recursive_mutex> lock(mutex);XSync(display,False);
    for(auto& item:windows){XWindowAttributes attributes{};if(XGetWindowAttributes(display,item.second.xid,&attributes))std::fprintf(stderr,"GE6_X11_WINDOW_SNAPSHOT hwnd=0x%lx xid=0x%lx width=%d height=%d viewable=%d procedure=0x%lx\n",item.first,item.second.xid,attributes.width,attributes.height,int(attributes.map_state==IsViewable),reinterpret_cast<uintptr_t>(item.second.procedure));}
    int width=DisplayWidth(display,0),height=DisplayHeight(display,0);if(width<=0||height<=0||width>8192||height>8192)return;
    auto* source=XGetImage(display,DefaultRootWindow(display),0,0,unsigned(width),unsigned(height),AllPlanes,ZPixmap);if(!source)return;
    std::vector<uint8_t> pixels(size_t(width)*size_t(height)*4);
    auto channel=[](unsigned long pixel,unsigned long mask){if(!mask)return uint8_t(0);while(!(mask&1)){mask>>=1;pixel>>=1;}return uint8_t(((pixel&mask)*255+mask/2)/mask);};
    for(int y=0;y<height;++y)for(int x=0;x<width;++x){auto value=XGetPixel(source,x,y);auto at=(size_t(y)*size_t(width)+size_t(x))*4;pixels[at]=channel(value,source->red_mask);pixels[at+1]=channel(value,source->green_mask);pixels[at+2]=channel(value,source->blue_mask);pixels[at+3]=255;}
    XDestroyImage(source);png_image image{};image.version=PNG_IMAGE_VERSION;image.width=unsigned(width);image.height=unsigned(height);image.format=PNG_FORMAT_RGBA;
    if(png_image_write_to_file(&image,path,0,pixels.data(),0,nullptr))std::fprintf(stderr,"GE6_X11_CAPTURE_SAVED width=%d height=%d path=%s\n",width,height,path);
    png_image_free(&image);
}
inline uintptr_t resolve(const std::string& library,const std::string& name){
    if(library=="SHELL32.dll"&&name=="DragAcceptFiles")return reinterpret_cast<uintptr_t>(&drag_files);
    if(library!="USER32.dll")return 0;
#define WINDOW_BIND(n,f) if(name==n)return reinterpret_cast<uintptr_t>(&f)
    WINDOW_BIND("MapVirtualKeyW",map_key);WINDOW_BIND("LoadCursorW",load_cursor);WINDOW_BIND("GetDoubleClickTime",double_click);
    WINDOW_BIND("AdjustWindowRectEx",adjust);WINDOW_BIND("CreateWindowExW",create);WINDOW_BIND("DefWindowProcW",default_proc);
    WINDOW_BIND("CallWindowProcW",call_proc);WINDOW_BIND("GetSystemMetrics",metrics);WINDOW_BIND("GetCursorPos",cursor_position);WINDOW_BIND("ScreenToClient",to_client);WINDOW_BIND("SetCursorPos",warp_cursor);WINDOW_BIND("GetForegroundWindow",focus);
    WINDOW_BIND("GetMenu",menu);
    WINDOW_BIND("SetCursor",set_cursor);WINDOW_BIND("ShowCursor",show_cursor);
    WINDOW_BIND("LoadIconA",load_icon_a);WINDOW_BIND("SendMessageA",send_a);
    WINDOW_BIND("GetPropW",get_property);WINDOW_BIND("SetPropW",set_property);WINDOW_BIND("RemovePropW",remove_property);
    WINDOW_BIND("GetWindowLongPtrW",get_long);WINDOW_BIND("SetWindowLongPtrW",set_long);WINDOW_BIND("GetWindowLongW",get_long32);WINDOW_BIND("SetWindowLongW",set_long32);
    WINDOW_BIND("GetClientRect",client_rect);WINDOW_BIND("GetWindowRect",window_rect);WINDOW_BIND("ClientToScreen",to_screen);WINDOW_BIND("GetDC",get_dc);WINDOW_BIND("ReleaseDC",release_dc);
    WINDOW_BIND("PeekMessageW",peek);WINDOW_BIND("TranslateMessage",translate);WINDOW_BIND("DispatchMessageW",dispatch);WINDOW_BIND("GetKeyState",key_state);WINDOW_BIND("GetFocus",focus);
    WINDOW_BIND("DestroyWindow",destroy);WINDOW_BIND("ShowWindow",show);WINDOW_BIND("SetWindowTextW",title);WINDOW_BIND("SetWindowPos",position);WINDOW_BIND("SendMessageW",send);
#undef WINDOW_BIND
    return 0;
}
#undef GE6_WIN_MS
}
#endif
