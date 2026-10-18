#include "script_runtime.h"
#include "xml_bridge.h"
#include "i18n_bootstrap.h"
#include "file_transport.h"
#ifdef _WIN32
#include <windows.h>
#include <objbase.h>
#endif
extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}
#include <algorithm>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>
namespace fs15portable {
namespace {
namespace fs=std::filesystem;
struct Platform {
#ifdef _WIN32
    Platform(){if(FAILED(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED)))throw std::runtime_error("Cannot initialize XML platform services");}
    ~Platform(){CoUninitialize();}
#endif
};
bool contained(const fs::path& target,const fs::path& base){auto a=target.begin(),b=base.begin();for(;b!=base.end();++a,++b)if(a==target.end()||*a!=*b)return false;return true;}
fs::path resolve(const fs::path& root,const std::vector<std::string>& bases,const std::string& name){
    if(name.empty()||name.find(':')!=std::string::npos||name.find('\\')!=std::string::npos)throw std::runtime_error("Invalid resource name");
    const auto relative=fs::u8path(name);if(relative.is_absolute())throw std::runtime_error("Absolute resource names are not supported");
    for(const auto& part:relative)if(part==".."||part==".")throw std::runtime_error("Resource path traversal rejected");
    for(const auto& base:bases){const auto directory=fs::weakly_canonical(root/base),candidate=fs::weakly_canonical(directory/relative);
        if(fs::is_regular_file(candidate)){if(!contained(candidate,directory))throw std::runtime_error("Resource escapes its root");return candidate;}}
    throw std::runtime_error("Missing required local source/resource: "+name);
}
std::string read(const fs::path& path){
    const auto body=fs15platform::read_file_string_bounded(path,8*1024*1024);
    if(body.empty())throw std::runtime_error("Empty script");
    return body;
}
void execute(lua_State* state,const std::string& body,const std::string& name,bool binary){
    if(!binary&&(static_cast<unsigned char>(body.front())==0x1b||body.find('\0')!=std::string::npos))throw std::runtime_error("Refusing bytecode or non-text source");
    if(luaL_loadbufferx(state,body.data(),body.size(),name.c_str(),binary?"b":"t")||lua_pcall(state,0,0,0)){
        const char* e=lua_tostring(state,-1);const std::string message=e?e:"Lua execution failed";lua_pop(state,1);throw std::runtime_error(message);
    }
}
struct Context {fs::path root;bool reference=false;size_t count=0;};
int source(lua_State* state){
    const char* name=luaL_checkstring(state,1);
    bool failed=false;{
        try{
            const std::string key=name;
            if(key!="dataS/scripts/shared/class.lua"&&key!="dataS/scripts/Utils.lua"&&key!="dataS/scripts/I18N.lua")throw std::runtime_error("Source outside localization bootstrap");
            auto& c=*static_cast<Context*>(lua_touserdata(state,lua_upvalueindex(1)));
            auto relative=key;if(c.reference)relative.replace(relative.size()-4,4,".luc");
            const auto path=resolve(c.root,c.reference?std::vector<std::string>{"recovered/luajit"}:std::vector<std::string>{"lua","recovered/lua"},relative);
            execute(state,read(path),"@"+key,c.reference);++c.count;
        }catch(const std::exception& e){lua_pushstring(state,e.what());failed=true;}
    }
    return failed?lua_error(state):0;
}
int erase(lua_State* state){fs15menu::delete_xml(static_cast<int>(luaL_checkinteger(state,1)));return 0;}
int print(lua_State* state){for(int i=1;i<=lua_gettop(state);++i){const char* text=lua_tostring(state,i);if(i>1)std::cerr<<'\t';std::cerr<<(text?text:"<non-string>");}std::cerr<<'\n';return 0;}
std::map<std::string,std::string> strings(lua_State* state,const char* field){
    std::map<std::string,std::string> out;lua_getfield(state,-1,field);if(!lua_istable(state,-1))throw std::runtime_error("Missing localization table");
    lua_pushnil(state);while(lua_next(state,-2)){
        if(lua_type(state,-2)!=LUA_TSTRING||lua_type(state,-1)!=LUA_TSTRING)throw std::runtime_error("Localization entry is not a string pair");
        size_t a=0,b=0;const char* key=lua_tolstring(state,-2,&a);const char* value=lua_tolstring(state,-1,&b);out.emplace(std::string(key,a),std::string(value,b));lua_pop(state,1);
    }lua_pop(state,1);return out;
}
double scalar(lua_State* state,const char* name){lua_getfield(state,-1,name);if(!lua_isnumber(state,-1))throw std::runtime_error("Missing localization factor");const double value=lua_tonumber(state,-1);lua_pop(state,1);return value;}
Localization load(const fs::path& root,const std::string& language,bool reference){
    if(language.size()!=2||!std::all_of(language.begin(),language.end(),[](char c){return c>='a'&&c<='z';}))throw std::invalid_argument("Language must be two lowercase ASCII letters");
    if(!fs::is_directory(root))throw std::invalid_argument("Source root is not a directory");
    Platform platform;(void)platform;Context context{root,reference,0};
    std::unique_ptr<lua_State,decltype(&lua_close)> state(luaL_newstate(),lua_close);if(!state)throw std::runtime_error("Cannot allocate LuaJIT VM");
    luaL_openlibs(state.get());lua_pushlightuserdata(state.get(),&context);lua_pushcclosure(state.get(),source,1);lua_setglobal(state.get(),"source");
    lua_register(state.get(),"delete",erase);lua_register(state.get(),"print",print);
    const auto suffix="_"+language;lua_pushlstring(state.get(),suffix.data(),suffix.size());lua_setglobal(state.get(),"g_languageSuffix");
    fs15menu::register_xml(state.get(),[&](const std::string& key){return resolve(root,{"assets","recovered/gar"},key);});
    execute(state.get(),fs15_i18n_bootstrap,"@authored-i18n-bootstrap",false);
    lua_getglobal(state.get(),"fs15_i18n_result");if(!lua_istable(state.get(),-1))throw std::runtime_error("Localization bootstrap returned no result");
    Localization result;result.texts=strings(state.get(),"texts");result.descriptions=strings(state.get(),"descriptions");result.labels=strings(state.get(),"labels");
    result.fluid=scalar(state.get(),"fluidFactor");result.power_hp=scalar(state.get(),"powerFactorHP");result.power_kw=scalar(state.get(),"powerFactorKW");result.loaded_sources=context.count;
    if(fs15menu::xml_document_count()!=0)throw std::runtime_error("Localization script retained XML handles");
    state.reset();if(fs15menu::xml_document_count()!=0)throw std::runtime_error("VM close retained XML handles");return result;
}
}
Localization load_localization(const std::filesystem::path& root,const std::string& language){return load(root,language,false);}
Localization reference_localization_for_test(const std::filesystem::path& root,const std::string& language){return load(root,language,true);}
const char* xml_backend(){
#ifdef _WIN32
    return "Windows XmlLite";
#else
    return "Expat";
#endif
}
}
