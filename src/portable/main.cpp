// Authored platform/CLI transport around recovered GE6 components.
// This is a headless portability probe, not native engine/game startup.
#include "scene_view.h"
#include "scene_view_lifecycle.h"
#include "wheel_forces.h"
#include "wheel_record.h"
#include "physics_worker.h"
#include "scene_graph.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
namespace fs=std::filesystem;
namespace {
std::string quoted(const std::string& value){
    std::string out="\"";constexpr char hex[]="0123456789abcdef";
    for(unsigned char c:value){
        if(c=='"'||c=='\\'){out+='\\';out+=static_cast<char>(c);}
        else if(c<0x20){out+="\\u00";out+=hex[c>>4];out+=hex[c&15];}
        else out+=static_cast<char>(c);
    }
    return out+'"';
}
const char* platform(){
#if defined(__linux__)
    return "linux";
#elif defined(_WIN32)
    return "windows";
#else
    return "other";
#endif
}
const char* architecture(){
#if defined(__aarch64__) || defined(_M_ARM64)
    return "arm64";
#elif defined(__x86_64__) || defined(_M_X64)
    return "x86_64";
#else
    return "other";
#endif
}
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
struct WheelWorkerProbe {
    const fs15::PhysicsManagerView* manager;const fs15::SceneObjectView* body;uint8_t* frame;
    std::thread::id caller=std::this_thread::get_id(),worker{};unsigned steps=0,collections=0;
    static int simulate(void* p,uintptr_t scene,float seconds,uint32_t iterations,float fixed_step){
        auto& b=*static_cast<WheelWorkerProbe*>(p);b.worker=std::this_thread::get_id();
        if(b.worker==b.caller||scene!=42||seconds<=0||iterations!=20||fixed_step!=1.f/60)return 0;
        ++b.steps;return fs15_script_wheel_forces(b.manager,b.body,b.frame,0x130,nullptr,nullptr);
    }
    static uintptr_t world(void*,uintptr_t scene){return scene==42?43:0;}
    static int collect(void* p,void* observations,uintptr_t token){auto& b=*static_cast<WheelWorkerProbe*>(p);
        if(!observations||token!=43||std::this_thread::get_id()!=b.worker||b.frame[0xf8]!=3||b.frame[0x108]!=3)return 0;
        ++b.collections;return 1;
    }
};
struct Probe {
    std::vector<void*> allocations;uint32_t next=0,registered=0;void* object=nullptr;bool transport_ok=true,constructed=false;
    unsigned maps=0,unmaps=0,worlds=0,creates=0,destroys=0;bool view_constructed=false;
    std::vector<float> buffers[2];
    FS15SceneCameraRecord camera{};FS15SceneSpatialGrid grid{};FS15SceneRenderView view{};FS15SceneViewContext context{};
    FS15SceneLifecycleServices memory{this,allocate,release,identifier,add,remove,nullptr,nullptr,nullptr};
    FS15SceneViewServices services{this,world,map,unmap,nullptr};
    FS15SceneViewLifetimeServices lifetime{this,create_buffer,destroy_buffer};
    static uintptr_t create_buffer(void* u,uint32_t bytes,uintptr_t initial,uint32_t flags){
        auto& p=*static_cast<Probe*>(u);const unsigned i=p.creates++;
        if(i>=2||bytes!=(i?176u:196u)||initial||flags!=8){p.transport_ok=false;return 0;}
        try{p.buffers[i].resize(bytes/4);}catch(...){return 0;}return i+1;
    }
    static void destroy_buffer(void* u,uintptr_t token,uint32_t deleting){
        auto& p=*static_cast<Probe*>(u);++p.destroys;
        if(token!=p.destroys||token>2||deleting!=1){p.transport_ok=false;return;}
        std::vector<float>().swap(p.buffers[token-1]);
    }
    static void* allocate(void* u,size_t n){
        auto& p=*static_cast<Probe*>(u);void* ptr=std::calloc(1,n);if(!ptr)return nullptr;
        try{p.allocations.push_back(ptr);}catch(...){std::free(ptr);return nullptr;}return ptr;
    }
    static void release(void* u,void* ptr){
        if(!ptr)return;
        auto& p=*static_cast<Probe*>(u);auto i=std::find(p.allocations.begin(),p.allocations.end(),ptr);
        if(i==p.allocations.end()){p.transport_ok=false;return;}
        p.allocations.erase(i);std::free(ptr);
    }
    static uint32_t identifier(void* u){return ++static_cast<Probe*>(u)->next;}
    static void add(void* u,uint32_t id,void* ptr){auto& p=*static_cast<Probe*>(u);if(p.registered)p.transport_ok=false;p.registered=id;p.object=ptr;}
    static void remove(void* u,uint32_t id,void* ptr){auto& p=*static_cast<Probe*>(u);if(p.registered!=id||p.object!=ptr)p.transport_ok=false;p.registered=0;p.object=nullptr;}
    static int world(void* u,FS15SceneStateRecord* node,float* out){
        auto& p=*static_cast<Probe*>(u);++p.worlds;if(node!=&p.camera.node||!fs15_scene_update_world(node,sizeof(p.camera),sizeof(p.camera),0))return 0;
        std::memcpy(out,node->world,64);return 1;
    }
    static float* map(void* u,uintptr_t token,size_t* count){
        auto& p=*static_cast<Probe*>(u);++p.maps;
        if(token!=1||p.view.volume!=reinterpret_cast<uintptr_t>(&p.view)||p.view.camera!=0)p.transport_ok=false;
        *count=p.buffers[0].size();return p.buffers[0].data();
    }
    static int unmap(void* u,uintptr_t token){auto& p=*static_cast<Probe*>(u);++p.unmaps;if(token!=1||p.view.camera!=0)p.transport_ok=false;return p.transport_ok?1:0;}
    ~Probe(){
        if(view_constructed)fs15_scene_view_destruct(&view,sizeof(view),&lifetime);
        if(grid.cells)fs15_scene_spatial_release(&grid,&memory);
        if(constructed)fs15_scene_camera_destruct(&camera,sizeof(camera),0,&memory);
        for(void* p:allocations)std::free(p);
    }
    void run(){
        constexpr char name[]="Portable host Camera";
        require(fs15_scene_camera_construct(&camera,sizeof(camera),name,sizeof(name),60,1,100,0,1,&memory),"Recovered Camera construction failed");constructed=true;
        context.width=1280;context.height=720;
        require(fs15_scene_view_construct(&view,sizeof(view),&context,sizeof(context),&lifetime),"Recovered render-view construction failed");view_constructed=true;
        require(view.buffer&&view.secondary_buffer&&creates==2,"Render-view buffers unavailable");
        require(fs15_scene_view_prepare(&view,sizeof(view),&camera,16.0f/9.0f,0,0,1250,&services),"Recovered Camera view preparation failed");
        require(maps==1&&unmaps==1&&worlds==1&&buffers[0][48]==1.25f&&view.camera==reinterpret_cast<uintptr_t>(&camera),"Camera buffer/identity transport failed");
        fs15::WheelRecord wheel{};wheel.shape_id=7;wheel.radius=0.5f;wheel.lateral_stiffness=2;wheel.reference_load=1;wheel.longitudinal_stiffness=8;wheel.friction=1.1f;
        fs15::WheelSet set{};set.count=1;set.records=&wheel;
        fs15::WheelTreeNode nil{},entry{};nil.is_nil=1;nil.parent=&entry;entry.left=entry.right=&nil;entry.actor_key=42;entry.value=&set;
        fs15::PhysicsManagerView manager{};manager.wheels=&nil;
        fs15::PhysicsActorData actor{};actor.flags=8;actor.actor_key=42;fs15::SceneObjectView body{};body.physics=&actor;
        uint8_t frame[0x130]{};const int32_t wheel_id=7;const float slip=0.2f,angle=-0.1f,load=2;
        std::memcpy(frame,&wheel_id,4);std::memcpy(frame+0x10,&slip,4);std::memcpy(frame+0x20,&angle,4);std::memcpy(frame+0x30,&load,4);
        require(fs15_script_wheel_forces(&manager,&body,frame,sizeof(frame),nullptr,nullptr),"Recovered registry-to-tire-force path failed");
        float longitudinal,lateral;std::memcpy(&longitudinal,frame+0xf0,4);std::memcpy(&lateral,frame+0x100,4);
        require(longitudinal>0&&lateral>0&&frame[0xf8]==3&&frame[0x108]==3,"Recovered tire-force frame invalid");
        WheelWorkerProbe backend{&manager,&body,frame};
        FS15PhysicsDispatchServices physics{&backend,WheelWorkerProbe::simulate,WheelWorkerProbe::world,WheelWorkerProbe::collect,nullptr,nullptr};
        fs15::platform::PhysicsWorker worker(42,1.f/60,20,physics);
        for(unsigned i=0;i<16;++i)require(worker.tick(16.f+i),"Recovered physics worker dispatch failed");
        require(worker.stop()&&worker.completed()==16&&backend.steps==16&&backend.collections==16,"Physics event/thread completion or shutdown failed");
        const float low[]={-256,-256,-256},high[]={256,256,256};
        require(fs15_scene_spatial_init(&grid,low,high,4,&memory),"Recovered spatial grid initialization failed");
        uint32_t tokens[2]{};FS15SceneSpatialSphere sphere{};sphere.center[2]=-5;sphere.radius=0.5f;
        require(fs15_scene_spatial_add(&grid,tokens,&sphere,1000,0,UINT32_MAX,&memory),"Visible sphere registration failed");sphere.center[0]=100;
        require(fs15_scene_spatial_add(&grid,tokens+1,&sphere,1000,0,UINT32_MAX,&memory),"Outside sphere registration failed");
        uint32_t* visible[2]{};uint32_t count=0;
        require(fs15_scene_spatial_volume_query(&grid,reinterpret_cast<void*>(view.volume),view.position,1,nullptr,visible,2,&count)&&count==1&&visible[0]==tokens,"Recovered Camera-to-grid visibility failed");
        require(fs15_scene_spatial_release(&grid,&memory),"Spatial allocation release failed");
        const bool view_destroyed=fs15_scene_view_destruct(&view,sizeof(view),&lifetime)!=0;view_constructed=false;
        require(view_destroyed&&destroys==2&&buffers[0].empty()&&buffers[1].empty(),"Render-view cleanup failed");
        const bool destroyed=fs15_scene_camera_destruct(&camera,sizeof(camera),0,&memory)!=0;constructed=false;
        require(destroyed,"Recovered Camera destruction failed");
        require(allocations.empty()&&!registered&&transport_ok,"Portable transport retained allocations or registry entries");
    }
};
struct Options {bool probe=false,help=false;fs::path game,report;};
Options parse(int argc,char** argv){
    Options o;
    for(int i=1;i<argc;++i){const std::string a=argv[i];
        if(a=="--help"||a=="-h")o.help=true;
        else if(a=="--probe"){if(o.probe)throw std::invalid_argument("Duplicate --probe");o.probe=true;}
        else if(a=="--game-dir"||a=="--report"){
            if(i+1==argc)throw std::invalid_argument("Option requires a path: "+a);
            auto& path=a=="--game-dir"?o.game:o.report;if(!path.empty())throw std::invalid_argument("Duplicate option: "+a);path=fs::u8path(argv[++i]);
            if(path.empty())throw std::invalid_argument("Empty option path: "+a);
        }else throw std::invalid_argument("Unknown option: "+a);
    }
    if(!o.probe&&(!o.game.empty()||!o.report.empty()))throw std::invalid_argument("--game-dir and --report require --probe");
    return o;
}
bool inside(const fs::path& path,const fs::path& root){auto a=path.begin(),b=root.begin();for(;b!=root.end();++a,++b)if(a==path.end()||*a!=*b)return false;return true;}
std::string resources(const fs::path& root){
    if(root.empty())return "null";
    if(!fs::is_directory(root))throw std::invalid_argument("Game directory does not exist or is not a directory");
    std::ostringstream out;out<<"{\"root\":"<<quoted(fs::weakly_canonical(root).u8string())<<",\"version_verified\":false,\"loaded_into_engine\":false,\"files\":[";
    bool first=true;for(const char* name:{"dataS.gar","dataS2.gar","data/maps/map01.i3d","data/maps/map02.i3d"}){
        if(!first)out<<',';
        first=false;const auto p=root/fs::u8path(name);const bool present=fs::is_regular_file(p);
        out<<"{\"path\":"<<quoted(name)<<",\"present\":"<<(present?"true":"false")<<",\"bytes\":";
        if(present)out<<fs::file_size(p);else out<<"null";out<<'}';
    }
    return out.str()+"]}";
}
void help(){std::cout<<"FS15 Portable Host - headless recovered-core probe\n"
    "Usage: FS15PortableHost --probe [--game-dir PATH] [--report FILE]\n"
    "Runs recovered Camera/view/shader/visibility/lifetime paths with authored\n"
    "CPU transport. Optional resource inspection is read-only and verifies no\n"
    "game version. This does not start GE6, render a window or run gameplay.\n";}
}
int main(int argc,char** argv){
    try{
        const Options o=parse(argc,argv);if(o.help||!o.probe){help();return 0;}
        const std::string inventory=resources(o.game);
        if(!o.game.empty()&&!o.report.empty()&&inside(fs::weakly_canonical(o.report),fs::weakly_canonical(o.game)))throw std::invalid_argument("Report must be outside the read-only game directory");
        Probe probe;probe.run();
        std::ostringstream result;result<<"{\n  \"schema_version\":1,\n  \"status\":\"passed\",\n  \"host_kind\":\"authored_headless_portability_host\",\n  \"platform\":"<<quoted(platform())<<",\n  \"architecture\":"<<quoted(architecture())
            <<",\n  \"native_engine_started\":false,\n  \"renderer_started\":false,\n  \"playable\":false,\n  \"physics_world_started\":false,\n  \"cpu_probe\":{\"view_constructed_and_destroyed\":true,\"buffers_created\":2,\"buffers_destroyed\":2,\"wheel_registry_force_adapter\":true,\"physics_worker_thread\":true,\"physics_worker_completed_steps\":16,\"camera_constructed_and_destroyed\":true,\"view_prepared\":true,\"shader_block_floats\":49,\"map_calls\":"<<probe.maps<<",\"unmap_calls\":"<<probe.unmaps<<",\"visible_objects\":1,\"outside_objects\":1,\"retained_allocations\":0},\n  \"resources\":"<<inventory<<"\n}\n";
        const std::string json=result.str();
        if(!o.report.empty()){
            std::ofstream stream(o.report,std::ios::binary|std::ios::trunc);if(!stream)throw std::runtime_error("Cannot open report file");stream<<json;stream.close();if(!stream)throw std::runtime_error("Cannot write report file");
        }
        std::cout<<json;return 0;
    }catch(const std::invalid_argument& e){std::cerr<<"Argument error: "<<e.what()<<'\n';return 2;}
    catch(const std::exception& e){std::cerr<<"Portable host failed: "<<e.what()<<'\n';return 1;}
}
