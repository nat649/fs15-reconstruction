#pragma once
// Included inside ge6_windows after scan_vk. Authored OS/ABI transport;
// physical scans use actual XKB key names, text uses the installed X layout.
struct HostKey {uint16_t scan=0,vk=0;};
inline std::array<HostKey,256> host_keys{};
inline std::array<bool,256> held_keys{};
inline thread_local std::array<uint8_t,256> keyboard_state{};
inline bool keyboard_ready=false,detectable_repeat=false;
inline bool init_keyboard(){
    if(keyboard_ready)return true;
    if(!connect())return false;
    auto* kb=XkbGetKeyboard(display,XkbAllComponentsMask,XkbUseCoreKbd);
    if(!kb||!kb->names||!kb->names->keys){if(kb)XkbFreeKeyboard(kb,XkbAllComponentsMask,True);set_error(50);return false;}
    std::map<std::string,uint16_t> scans;
    auto row=[&](const char* prefix,int first,int count){for(int i=0;i<count;++i){char name[16]{};std::snprintf(name,sizeof(name),"%s%02d",prefix,i+1);scans[name]=uint16_t(first+i);}};
    row("AE",2,12);row("AD",16,12);row("AC",30,11);row("AB",44,10);row("FK",59,10);
    for(auto item:{std::pair<const char*,uint16_t>{"ESC",1},{"BKSP",14},{"TAB",15},{"RTRN",28},{"LCTL",29},{"TLDE",41},{"LFSH",42},{"BKSL",43},{"RTSH",54},{"KPMU",55},{"LALT",56},{"SPCE",57},{"CAPS",58},{"NMLK",69},{"SCLK",70},{"KP7",71},{"KP8",72},{"KP9",73},{"KPSU",74},{"KP4",75},{"KP5",76},{"KP6",77},{"KPAD",78},{"KP1",79},{"KP2",80},{"KP3",81},{"KP0",82},{"KPDL",83},{"LSGT",86},{"FK11",87},{"FK12",88},{"KPEN",0x11c},{"RCTL",0x11d},{"KPDV",0x135},{"PRSC",0x137},{"RALT",0x138},{"HOME",0x147},{"UP",0x148},{"PGUP",0x149},{"LEFT",0x14b},{"RGHT",0x14d},{"END",0x14f},{"DOWN",0x150},{"PGDN",0x151},{"INS",0x152},{"DELE",0x153},{"LWIN",0x15b},{"RWIN",0x15c},{"MENU",0x15d}})scans[item.first]=item.second;
    for(unsigned i=kb->min_key_code;i<=kb->max_key_code;++i){
        std::string name(kb->names->keys[i].name,4);while(!name.empty()&&(name.back()==' '||name.back()==0))name.pop_back();auto it=scans.find(name);if(it==scans.end())continue;
        uint16_t scan=it->second,vk=scan<128?scan_vk[scan]:0;
        switch(scan){case 0x11c:vk=13;break;case 0x11d:vk=0x11;break;case 0x135:vk=0x6f;break;case 0x137:vk=0x2c;break;case 0x138:vk=0x12;break;case 0x147:vk=0x24;break;case 0x148:vk=0x26;break;case 0x149:vk=0x21;break;case 0x14b:vk=0x25;break;case 0x14d:vk=0x27;break;case 0x14f:vk=0x23;break;case 0x150:vk=0x28;break;case 0x151:vk=0x22;break;case 0x152:vk=0x2d;break;case 0x153:vk=0x2e;break;case 0x15b:vk=0x5b;break;case 0x15c:vk=0x5c;break;case 0x15d:vk=0x5d;break;default:break;}
        auto sym=XkbKeycodeToKeysym(display,KeyCode(i),0,0);if(sym>='a'&&sym<='z')vk=uint16_t(sym-'a'+'A');else if(sym>='A'&&sym<='Z')vk=uint16_t(sym);
        host_keys[i]={scan,vk};
    }
    XkbFreeKeyboard(kb,XkbAllComponentsMask,True);Bool supported=False;XkbSetDetectableAutoRepeat(display,True,&supported);detectable_repeat=supported;
    keyboard_ready=true;return true;
}
inline uint16_t sided_key(HostKey key){if(key.vk==0x10)return key.scan==54?0xa1:0xa0;if(key.vk==0x11)return key.scan&0x100?0xa3:0xa2;if(key.vk==0x12)return key.scan&0x100?0xa5:0xa4;return key.vk;}
inline void consume_key(uint32_t message,uintptr_t vk,intptr_t lp){
    const bool down=message==0x100||message==0x104;
    HostKey key{uint16_t(((uintptr_t(lp)>>16)&255)|((uintptr_t(lp)&(1u<<24))?0x100:0)),uint16_t(vk)};auto side=sided_key(key);
    if(side>=256)return;
    auto& value=keyboard_state[side];if(down&&!(value&0x80)&&(side==20||side==0x90||side==0x91))value^=1;
    value=uint8_t((value&1)|(down?0x80:0));
    for(auto group:{std::pair<int,int>{0x10,0xa0},{0x11,0xa2},{0x12,0xa4}})keyboard_state[group.first]=uint8_t((keyboard_state[group.second]|keyboard_state[group.second+1])&0x80);
}
inline GE6_WIN_MS int keyboard_snapshot(uint8_t* result){trace("GetKeyboardState");if(!result){set_error(87);return 0;}std::copy(keyboard_state.begin(),keyboard_state.end(),result);return 1;}
inline GE6_WIN_MS int16_t key_state(int code){trace("GetKeyState");if(code<0||code>255){set_error(87);return 0;}auto value=keyboard_state[size_t(code)];return int16_t((value&0x80?0x8000:0)|(value&1));}
inline bool physical_key_down(int code){
    if(!init_keyboard())return false;
    char keys[32]{};XQueryKeymap(display,keys);
    for(size_t i=0;i<host_keys.size();++i)if((host_keys[i].vk==code||sided_key(host_keys[i])==code)&&(keys[i/8]&(1<<(i%8))))return true;
    return false;
}
inline GE6_WIN_MS int to_unicode(uint32_t vk,uint32_t scan,const uint8_t* state,uint16_t* result,int count,uint32_t flags){
    trace("ToUnicode");std::lock_guard<std::recursive_mutex> lock(mutex);
    if(!state||!result||count<=0||vk>255||flags){set_error(87);return 0;}if(scan&0x8000)return 0;if(!init_keyboard())return 0;
    unsigned keycode=0;for(size_t i=0;i<host_keys.size();++i)if(host_keys[i].vk==vk&&(host_keys[i].scan&255)==(scan&255)){keycode=unsigned(i);break;}if(!keycode)return 0;
    unsigned mods=0;if(state[0x10]&0x80)mods|=ShiftMask;if(state[20]&1)mods|=LockMask;if(state[0x11]&0x80)mods|=ControlMask;if(state[0x12]&0x80)mods|=Mod1Mask;if(state[0x90]&1)mods|=Mod2Mask;
    KeySym sym=0;unsigned consumed=0;if(!XkbLookupKeySym(display,KeyCode(keycode),mods,&consumed,&sym))return 0;
    uint32_t character=0;
    if(sym>=32&&sym<=255)character=uint32_t(sym);else if((sym&0xff000000)==0x01000000)character=uint32_t(sym&0xffffff);
    else switch(sym){case XK_Return:case XK_KP_Enter:character=13;break;case XK_Tab:character=9;break;case XK_BackSpace:character=8;break;case XK_Escape:character=27;break;default:break;}
    // Dead-key composition, AltGr and IME are outside this bounded backend.
    if(!character||character>0x10ffff||(character>=0xd800&&character<=0xdfff))return 0;
    if((mods&ControlMask)&&character>='@'&&character<='_')character&=31;else if((mods&ControlMask)&&character>='a'&&character<='z')character=character-'a'+1;
    if(character<=0xffff){result[0]=uint16_t(character);return 1;}if(count<2){set_error(122);return 0;}character-=0x10000;result[0]=uint16_t(0xd800+(character>>10));result[1]=uint16_t(0xdc00+(character&1023));return 2;
}
