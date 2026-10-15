#include "mission.h"
#include "scene_render.h"
#include "foliage.h"
#include "collision.h"
#include "simulation_clock.h"
#include "../menu/xml_bridge.h"
#include <lua.hpp>
#include <DirectXCollision.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

using namespace DirectX;
namespace fs=std::filesystem;
struct MissionRuntime::Impl {
    fs::path root;
    const fs15world::Terrain& terrain;
    lua_State* lua=nullptr;
    struct Body {fs15world::Terrain scene;std::unique_ptr<SceneRenderer> renderer;XMFLOAT3 position{};float yaw=0;std::vector<int> handles;};
    struct Ref {fs15world::Terrain* scene;int index;};
    std::vector<Body> bodies;
    std::map<int,Ref> nodes;
    std::map<int,int> map_handles;
    std::set<fs::path> used;
    std::vector<fs::path> resources;
    int next_handle=1000000;
    int controlled=0;
    float orbit_yaw=0,orbit_pitch=0.35f,camera_distance=10;
    fs15world::DensityGrid fruit,soil;
    std::unique_ptr<FoliageRenderer> foliage;
    std::unique_ptr<CollisionWorld> collision;
    struct Filter {std::string op="greater";int minimum=-1,maximum=0,include=0,exclude=0;};
    struct DensityView {fs15world::DensityGrid* grid=nullptr;unsigned offset=0,type=0;int type_compare=0,new_type=0,shift=0;Filter compare,mask;};
    std::map<int,DensityView> density_views;
    unsigned revision=0,rendered_revision=0;
    SimulationClock clock;
    bool reference_bytecode=false;
    unsigned reference_chunks=0;
    // Scene nodes remain stable once a vehicle has been parsed.
    fs::path resolve(const std::string& name){
        const auto rel=fs::u8path(name);if(rel.is_absolute()||rel.has_root_name())throw std::runtime_error("Absolute mission resource");for(const auto& part:rel)if(part=="..")throw std::runtime_error("Mission resource traversal");
        for(const auto& prefix:{root/"assets",root/"recovered/gar",root/"recovered/lua",root/"lua",root})if(fs::is_regular_file(prefix/rel)){used.insert(rel);return prefix/rel;}
        throw std::runtime_error("Missing mission resource: "+name);
    }
    void call(const char* function,int args=0,int returns=0){if(lua_pcall(lua,args,returns,0))throw std::runtime_error(std::string(function)+": "+lua_tostring(lua,-1));}
    void execute(const fs::path& file){if(luaL_loadfile(lua,file.u8string().c_str()))throw std::runtime_error(lua_tostring(lua,-1));call("load mission source");}
    Ref node(int handle){auto at=nodes.find(handle);if(at==nodes.end())throw std::runtime_error("Unknown mission scene node");return at->second;}
    int map_handle(int index){auto at=map_handles.find(index);if(at!=map_handles.end())return at->second;int h=++next_handle;map_handles[index]=h;nodes[h]={const_cast<fs15world::Terrain*>(&terrain),index};return h;}
    int handle(Ref ref,int index){if(ref.scene==&terrain)return map_handle(index);for(const auto& b:bodies)if(&b.scene==ref.scene)return b.handles.at(index);throw std::runtime_error("Unknown vehicle scene");}
    static Impl& self(lua_State* l){return *static_cast<Impl*>(lua_touserdata(l,lua_upvalueindex(1)));}
    template<int (*Fn)(lua_State*)>static int safe(lua_State* l){try{return Fn(l);}catch(const std::exception& e){lua_pushstring(l,e.what());}return lua_error(l);}
    void bind(const char* name,lua_CFunction fn){lua_pushlightuserdata(lua,this);lua_pushcclosure(lua,fn,1);lua_setglobal(lua,name);}
    static int source(lua_State* l){
        auto& s=self(l);std::string name=luaL_checkstring(l,1);auto file=s.resolve(name);
        if(s.reference_bytecode){
            auto relative=fs::u8path(name);
            if(relative.begin()==relative.end()||*relative.begin()!=L"dataS"||relative.extension()!=L".lua")throw std::runtime_error("Unsupported reference Lua resource");
            relative.replace_extension(".luc");file=s.root/"recovered/luajit"/relative;
            if(!fs::is_regular_file(file))throw std::runtime_error("Missing local reference Lua bytecode: "+name);
            s.execute(file);++s.reference_chunks;
        }else s.execute(file);
        return 0;
    }
    static int read_source(lua_State* l){auto& s=self(l);std::ifstream f(s.resolve(luaL_checkstring(l,1)),std::ios::binary);std::string text((std::istreambuf_iterator<char>(f)),{});lua_pushlstring(l,text.data(),text.size());return 1;}
    static int destroy_xml(lua_State* l){fs15menu::delete_xml(int(luaL_checkinteger(l,1)));return 0;}
    static int load_vehicle(lua_State* l){auto& s=self(l);if(s.bodies.size()>=64)throw std::runtime_error("Starting fleet exceeds limit");fs::path file=fs::u8path(luaL_checkstring(l,1));file.replace_extension(".runtime.i3d");
        Body b;b.scene=fs15world::load_scene(s.root,file,true);for(const auto& r:b.scene.used_resources)s.used.insert(r);
        for(size_t i=0;i<b.scene.nodes.size();++i)b.handles.push_back(++s.next_handle);
        s.bodies.push_back(std::move(b));
        // Vector growth moves Body objects; refresh all registered scene pointers.
        for(auto& body:s.bodies)for(size_t i=0;i<body.handles.size();++i)s.nodes[body.handles[i]]={&body.scene,int(i)};
        auto& added=s.bodies.back();lua_pushinteger(l,s.bodies.size());lua_newtable(l);int n=0;for(size_t i=0;i<added.scene.nodes.size();++i)if(added.scene.nodes[i].parent<0){lua_pushinteger(l,added.handles[i]);lua_rawseti(l,-2,++n);}return 2;
    }
    static int children(lua_State* l){auto& s=self(l);auto r=s.node(int(luaL_checkinteger(l,1)));lua_pushinteger(l,r.scene->nodes[r.index].children.size());return 1;}
    static int child(lua_State* l){auto& s=self(l);auto r=s.node(int(luaL_checkinteger(l,1)));int i=int(luaL_checkinteger(l,2));const auto& c=r.scene->nodes[r.index].children;if(i<0||size_t(i)>=c.size())throw std::runtime_error("Mission child index out of range");lua_pushinteger(l,s.handle(r,c[i]));return 1;}
    static int translation(lua_State* l){auto& s=self(l);auto r=s.node(int(luaL_checkinteger(l,1)));for(float v:r.scene->nodes[r.index].translation)lua_pushnumber(l,v);return 3;}
    static int rotation(lua_State* l){auto& s=self(l);auto r=s.node(int(luaL_checkinteger(l,1)));for(float v:r.scene->nodes[r.index].rotation)lua_pushnumber(l,v);return 3;}
    static int scale(lua_State* l){auto& s=self(l);auto r=s.node(int(luaL_checkinteger(l,1)));for(float v:r.scene->nodes[r.index].scale)lua_pushnumber(l,v);return 3;}
    static int visibility(lua_State* l){auto& s=self(l);auto r=s.node(int(luaL_checkinteger(l,1)));lua_pushboolean(l,r.scene->nodes[r.index].local_visible);return 1;}
    static int set_visibility(lua_State* l){auto& s=self(l);auto r=s.node(int(luaL_checkinteger(l,1)));r.scene->nodes[r.index].local_visible=lua_toboolean(l,2)!=0;return 0;}
    static int set_scale(lua_State* l){auto& s=self(l);auto r=s.node(int(luaL_checkinteger(l,1)));if(r.scene==&s.terrain)throw std::runtime_error("Map transforms are read-only");for(int k=0;k<3;++k){double value=luaL_checknumber(l,k+2);if(!std::isfinite(value)||value<=0)throw std::runtime_error("Animation scale must be finite and positive");r.scene->nodes[r.index].scale[k]=float(value);}return 0;}
    static int parent(lua_State* l){auto& s=self(l);auto r=s.node(int(luaL_checkinteger(l,1)));int p=r.scene->nodes[r.index].parent;if(p<0)lua_pushnil(l);else lua_pushinteger(l,s.handle(r,p));return 1;}
    static int name(lua_State* l){auto& s=self(l);auto r=s.node(int(luaL_checkinteger(l,1)));lua_pushstring(l,r.scene->nodes[r.index].name.c_str());return 1;}
    static int set_translation(lua_State* l){auto& s=self(l);auto r=s.node(int(luaL_checkinteger(l,1)));if(r.scene==&s.terrain)throw std::runtime_error("Map transforms are read-only");for(int k=0;k<3;++k)r.scene->nodes[r.index].translation[k]=float(luaL_checknumber(l,k+2));return 0;}
    static int set_rotation(lua_State* l){auto& s=self(l);auto r=s.node(int(luaL_checkinteger(l,1)));if(r.scene==&s.terrain)throw std::runtime_error("Map transforms are read-only");for(int k=0;k<3;++k)r.scene->nodes[r.index].rotation[k]=float(luaL_checknumber(l,k+2));return 0;}
    static int transform(lua_State* l){auto& s=self(l);auto r=s.node(int(luaL_checkinteger(l,1)));XMFLOAT4X4 w;std::copy(r.scene->nodes[r.index].world,r.scene->nodes[r.index].world+16,&w._11);auto v=XMVectorSet(float(luaL_optnumber(l,2,0)),float(luaL_optnumber(l,3,0)),float(luaL_optnumber(l,4,0)),1);auto result=lua_toboolean(l,lua_upvalueindex(2))?XMVector3TransformNormal(v,XMLoadFloat4x4(&w)):XMVector3TransformCoord(v,XMLoadFloat4x4(&w));XMFLOAT3 p;XMStoreFloat3(&p,result);lua_pushnumber(l,p.x);lua_pushnumber(l,p.y);lua_pushnumber(l,p.z);return 3;}
    static int direction(lua_State* l){auto& s=self(l);auto r=s.node(int(luaL_checkinteger(l,1)));XMFLOAT4X4 w;std::copy(r.scene->nodes[r.index].world,r.scene->nodes[r.index].world+16,&w._11);XMFLOAT3 p;XMStoreFloat3(&p,XMVector3TransformNormal(XMVectorSet(float(luaL_checknumber(l,2)),float(luaL_checknumber(l,3)),float(luaL_checknumber(l,4)),0),XMLoadFloat4x4(&w)));lua_pushnumber(l,p.x);lua_pushnumber(l,p.y);lua_pushnumber(l,p.z);return 3;}
    static int attribute(lua_State* l){auto& s=self(l);auto r=s.node(int(luaL_checkinteger(l,1)));const auto& node=r.scene->nodes[r.index];const std::string key=luaL_checkstring(l,2);auto at=node.attributes.find(key);if(at==node.attributes.end()){lua_pushnil(l);return 1;}
        auto type=node.attribute_types.find(key);const std::string kind=type==node.attribute_types.end()?"string":type->second;
        if(kind=="boolean"){if(at->second!="true"&&at->second!="false")throw std::runtime_error("Invalid original boolean user attribute");lua_pushboolean(l,at->second=="true");}
        else if(kind=="float"||kind=="integer"){size_t end=0;double value=std::stod(at->second,&end);if(end!=at->second.size()||!std::isfinite(value)||(kind=="integer"&&std::floor(value)!=value))throw std::runtime_error("Invalid original numeric user attribute");lua_pushnumber(l,value);}
        else lua_pushstring(l,at->second.c_str());return 1;
    }
    static int height(lua_State* l){auto& s=self(l);lua_pushnumber(l,fs15world::terrain_height(s.terrain,float(luaL_checknumber(l,1)),float(luaL_checknumber(l,2))));return 1;}
    static int pose(lua_State* l){auto& s=self(l);int i=int(luaL_checkinteger(l,1))-1;if(i<0||size_t(i)>=s.bodies.size())throw std::runtime_error("Invalid vehicle index");auto& b=s.bodies[i];b.position={float(luaL_checknumber(l,2)),float(luaL_checknumber(l,3)),float(luaL_checknumber(l,4))};b.yaw=float(luaL_checknumber(l,6));XMFLOAT4X4 w;XMStoreFloat4x4(&w,XMMatrixRotationX(float(luaL_checknumber(l,5)))*XMMatrixRotationY(b.yaw)*XMMatrixRotationZ(float(luaL_checknumber(l,7)))*XMMatrixTranslation(b.position.x,b.position.y,b.position.z));fs15world::Matrix m;std::copy(&w._11,&w._11+16,m.value);fs15world::update_transforms(b.scene,m);return 0;}
    static int find_callbacks(lua_State* l){auto& s=self(l);std::string callback=luaL_checkstring(l,1);lua_newtable(l);int count=0;for(size_t i=0;i<s.terrain.nodes.size();++i){auto at=s.terrain.nodes[i].attributes.find("onCreate");if(at!=s.terrain.nodes[i].attributes.end()&&at->second==callback){lua_pushinteger(l,s.map_handle(int(i)));lua_rawseti(l,-2,++count);}}return 1;}
    static BoundingBox box(const Ref& r){const auto& node=r.scene->nodes[r.index];if(node.mesh<0)throw std::runtime_error("Trigger needs an original mesh");const auto& vertices=r.scene->meshes[node.mesh].vertices;BoundingBox local,world;BoundingBox::CreateFromPoints(local,vertices.size(),reinterpret_cast<const XMFLOAT3*>(vertices.data()),sizeof(fs15world::Vertex));XMFLOAT4X4 w;std::copy(node.world,node.world+16,&w._11);local.Transform(world,XMLoadFloat4x4(&w));return world;}
    static int overlap(lua_State* l){auto& s=self(l);auto a=s.node(int(luaL_checkinteger(l,1))),b=s.node(int(luaL_checkinteger(l,2)));lua_pushboolean(l,box(a).Intersects(box(b)));return 1;}
    static int bounds(lua_State* l){auto& s=self(l);auto b=box(s.node(int(luaL_checkinteger(l,1))));for(float v:{b.Center.x,b.Center.y,b.Center.z,b.Extents.x,b.Extents.y,b.Extents.z})lua_pushnumber(l,v);return 6;}
    static int raycast(lua_State* l){auto& s=self(l);XMFLOAT3 origin{float(luaL_checknumber(l,1)),float(luaL_checknumber(l,2)),float(luaL_checknumber(l,3))},direction{float(luaL_checknumber(l,4)),float(luaL_checknumber(l,5)),float(luaL_checknumber(l,6))};const std::string callback=luaL_checkstring(l,7);float limit=float(luaL_checknumber(l,8));auto ray=XMVector3Normalize(XMLoadFloat3(&direction));
        std::vector<std::pair<float,int>> hits;lua_getglobal(l,"g_currentMission");lua_getfield(l,-1,"nodeToVehicle");
        for(const auto& body:s.bodies)for(size_t i=0;i<body.handles.size();++i){lua_rawgeti(l,-1,body.handles[i]);bool registered=!lua_isnil(l,-1);lua_pop(l,1);if(!registered||body.scene.nodes[i].mesh<0)continue;float distance=0;if(box(s.node(body.handles[i])).Intersects(XMLoadFloat3(&origin),ray,distance)&&distance<=limit)hits.emplace_back(distance,body.handles[i]);}
        lua_pop(l,2);std::sort(hits.begin(),hits.end());
        for(const auto& hit:hits){XMFLOAT3 p;XMStoreFloat3(&p,XMLoadFloat3(&origin)+ray*hit.first);lua_getfield(l,9,callback.c_str());lua_pushvalue(l,9);lua_pushinteger(l,hit.second);lua_pushnumber(l,p.x);lua_pushnumber(l,p.y);lua_pushnumber(l,p.z);lua_pushnumber(l,hit.first);s.call("vehicle raycast callback",6,1);bool keep=lua_toboolean(l,-1)!=0;lua_pop(l,1);if(!keep)break;}return 0;
    }
    static int select(lua_State* l){auto& s=self(l);s.controlled=int(luaL_checkinteger(l,1));return 0;}
    static int move(lua_State* l){auto& s=self(l);float x=float(luaL_checknumber(l,1)),z=float(luaL_checknumber(l,2)),tx=float(luaL_checknumber(l,3)),tz=float(luaL_checknumber(l,4)),radius=float(luaL_optnumber(l,5,0.85));float distance=std::hypot(tx-x,tz-z);if(!std::isfinite(distance)||distance>100)throw std::runtime_error("Vehicle move exceeds collision service limit");unsigned steps=std::max(1u,unsigned(std::ceil(distance/0.25f)));bool blocked=false;float dx=(tx-x)/steps,dz=(tz-z)/steps;
        for(unsigned i=0;i<steps;++i){float nx=x+dx,nz=z+dz;float ground=fs15world::terrain_height(s.terrain,nx,nz);if(s.collision->blocked(nx,nz,ground,radius)){blocked=true;break;}x=nx;z=nz;}
        lua_pushnumber(l,x);lua_pushnumber(l,z);lua_pushboolean(l,blocked);return 3;
    }
    static int crop_info(lua_State* l){auto& s=self(l);float x=float(luaL_checknumber(l,1)),z=float(luaL_checknumber(l,2));int px=int((x/s.terrain.width+0.5f)*s.fruit.width),pz=int((z/s.terrain.depth+0.5f)*s.fruit.height);unsigned value=0;if(px>=0&&pz>=0&&px<int(s.fruit.width)&&pz<int(s.fruit.height))value=s.fruit.values[size_t(pz)*s.fruit.width+px];for(const auto& layer:s.terrain.foliage)if(layer.offset==4&&layer.type==(value&15)){lua_pushstring(l,layer.name.c_str());lua_pushinteger(l,(value>>4)&15);return 2;}lua_pushnil(l);lua_pushinteger(l,0);return 2;}
    static int crop_layers(lua_State* l){auto& s=self(l);lua_newtable(l);int n=0;for(const auto& layer:s.terrain.foliage)if(layer.offset==4){lua_newtable(l);lua_pushstring(l,layer.name.c_str());lua_setfield(l,-2,"name");lua_pushinteger(l,100+layer.type);lua_setfield(l,-2,"id");lua_rawseti(l,-2,++n);}return 1;}
    DensityView& density(int id){auto at=density_views.find(id);if(at==density_views.end())throw std::runtime_error("Unknown density layer");return at->second;}
    static bool matches(const Filter& f,int v){if(f.exclude&&(v&f.exclude))return false;if(f.include)v&=f.include;if(f.op=="between")return v>=f.minimum&&v<=f.maximum;if(f.op=="equals")return v==f.minimum;if(f.op=="greater")return v>f.minimum;if(f.op=="less")return v<f.minimum;throw std::runtime_error("Unsupported density comparison");}
    static int density_compare(lua_State* l){auto& s=self(l);auto& v=s.density(int(luaL_checkinteger(l,1)));v.compare={luaL_checkstring(l,2),int(luaL_checkinteger(l,3)),int(luaL_optinteger(l,4,0)),int(luaL_optinteger(l,5,0)),int(luaL_optinteger(l,6,0))};return 0;}
    static int density_mask(lua_State* l){auto& s=self(l);auto& v=s.density(int(luaL_checkinteger(l,1)));v.mask={luaL_checkstring(l,2),int(luaL_checkinteger(l,3)),int(luaL_optinteger(l,4,0)),int(luaL_optinteger(l,5,0)),int(luaL_optinteger(l,6,0))};return 0;}
    static int density_shift(lua_State* l){auto& s=self(l);s.density(int(luaL_checkinteger(l,1))).shift=int(luaL_checkinteger(l,2));return 0;}
    static int type_compare(lua_State* l){auto& s=self(l);s.density(int(luaL_checkinteger(l,1))).type_compare=int(luaL_checkinteger(l,2));return 0;}
    static int new_type(lua_State* l){auto& s=self(l);s.density(int(luaL_checkinteger(l,1))).new_type=int(luaL_checkinteger(l,2));return 0;}
    static int density_area(lua_State* l){
        auto& s=self(l);auto& v=s.density(int(luaL_checkinteger(l,1)));const bool write=lua_toboolean(l,lua_upvalueindex(2))!=0,masked=lua_toboolean(l,lua_upvalueindex(3))!=0;
        double x=luaL_checknumber(l,2),z=luaL_checknumber(l,3),wx=luaL_checknumber(l,4),wz=luaL_checknumber(l,5),hx=luaL_checknumber(l,6),hz=luaL_checknumber(l,7);
        unsigned start=unsigned(luaL_checkinteger(l,8))+v.offset,bits=unsigned(luaL_checkinteger(l,9));
        if(!bits||bits>16||start+bits>16)throw std::runtime_error("Density channel range exceeds supported grid");
        auto mask=(1u<<bits)-1;double det=wx*hz-wz*hx,sum=0,count=0,total=0;
        if(std::abs(det)<1e-9){lua_pushnumber(l,0);lua_pushnumber(l,0);lua_pushnumber(l,0);return 3;}
        DensityView* mv=nullptr;unsigned ms=0,mb=0;int value=0;
        if(masked){mv=&s.density(int(luaL_checkinteger(l,10)));ms=unsigned(luaL_checkinteger(l,11))+mv->offset;mb=unsigned(luaL_checkinteger(l,12));if(!mb||ms+mb>16)throw std::runtime_error("Density mask channel range");value=int(luaL_checkinteger(l,13));}
        else if(write)value=int(luaL_checkinteger(l,10));
        if(write&&(value<0||unsigned(value)>mask))throw std::runtime_error("Density value exceeds channel range");
        const auto& g=*v.grid;double scale_x=g.width/s.terrain.width,scale_z=g.height/s.terrain.depth;
        int min_x=std::max(0,int(std::floor((std::min({x,x+wx,x+hx,x+wx+hx})+s.terrain.width/2)*scale_x))),max_x=std::min(int(g.width)-1,int(std::ceil((std::max({x,x+wx,x+hx,x+wx+hx})+s.terrain.width/2)*scale_x)));
        int min_z=std::max(0,int(std::floor((std::min({z,z+wz,z+hz,z+wz+hz})+s.terrain.depth/2)*scale_z))),max_z=std::min(int(g.height)-1,int(std::ceil((std::max({z,z+wz,z+hz,z+wz+hz})+s.terrain.depth/2)*scale_z)));
        for(int zz=min_z;zz<=max_z;++zz)for(int xx=min_x;xx<=max_x;++xx){double dx=(xx+0.5)/scale_x-s.terrain.width/2-x,dz=(zz+0.5)/scale_z-s.terrain.depth/2-z;double u=(dx*hz-dz*hx)/det,t=(wx*dz-wz*dx)/det;if(u<0||u>=1||t<0||t>=1)continue;++total;
            auto& raw=v.grid->values[size_t(zz)*g.width+xx];int old=(raw>>start)&mask;bool same=!v.type||unsigned(raw&15)==v.type;
            if(v.type_compare==0&&!same&&(raw&15))continue;if(v.type_compare==1&&same)continue;
            if(!same&&v.type)old=0;
            if(!matches(v.compare,old))continue;
            if(mv){unsigned mx=unsigned(xx)*mv->grid->width/g.width,mz=unsigned(zz)*mv->grid->height/g.height;auto mraw=mv->grid->values[size_t(mz)*mv->grid->width+mx];int mvalue=(mraw>>ms)&((1u<<mb)-1);if(mv->type&&unsigned(mraw&15)!=mv->type)mvalue=0;if(!matches(v.mask,mvalue))continue;}
            sum+=old+v.shift;++count;
            if(write){raw=std::uint16_t((raw&~(mask<<start))|(unsigned(value)<<start));if(v.type){unsigned type=v.new_type==2?0:v.new_type==1?(raw&15):v.type;raw=std::uint16_t((raw&~15)|type);}}
        }
        if(write&&count>0)++s.revision;lua_pushnumber(l,sum);lua_pushnumber(l,count);lua_pushnumber(l,total);return 3;
    }
    void bind_area(const char* name,bool write,bool masked){lua_pushlightuserdata(lua,this);lua_pushboolean(lua,write);lua_pushboolean(lua,masked);lua_pushcclosure(lua,safe<density_area>,3);lua_setglobal(lua,name);}
    Impl(const fs::path& r,const fs15world::Terrain& t,int difficulty,bool reference):root(r),terrain(t),reference_bytecode(reference){
        bodies.reserve(64);lua=luaL_newstate();if(!lua)throw std::runtime_error("Cannot create mission Lua VM");luaL_openlibs(lua);
        fs15menu::register_xml(lua,[this](const std::string& name){return resolve(name);});
        bind("source",safe<source>);bind("worldReadSource",safe<read_source>);bind("delete",safe<destroy_xml>);bind("worldLoadVehicle",safe<load_vehicle>);bind("getNumOfChildren",safe<children>);bind("getChildAt",safe<child>);bind("getTranslation",safe<translation>);bind("getRotation",safe<rotation>);bind("setRotation",safe<set_rotation>);bind("getWorldTranslation",safe<transform>);bind("localToWorld",safe<transform>);bind("localDirectionToWorld",safe<direction>);bind("getUserAttribute",safe<attribute>);bind("worldHeight",safe<height>);bind("worldSetPose",safe<pose>);bind("worldFindCallbacks",safe<find_callbacks>);bind("worldOverlap",safe<overlap>);bind("worldSelect",safe<select>);
        collision=std::make_unique<CollisionWorld>(t);bind("raycastAll",safe<raycast>);bind("worldMove",safe<move>);bind("worldBounds",safe<bounds>);bind("getParent",safe<parent>);bind("getName",safe<name>);bind("setTranslation",safe<set_translation>);
        bind("getScale",safe<scale>);bind("setScale",safe<set_scale>);bind("getVisibility",safe<visibility>);bind("setVisibility",safe<set_visibility>);
        auto map_dir=fs::path("data/maps")/(t.map_id=="Map01"?"map01":"map02");fruit=fs15world::load_grle(resolve((map_dir/"fruit_density.grle").generic_u8string()));soil=fs15world::load_grle(resolve((map_dir/"cultivator_density.grle").generic_u8string()));density_views[1].grid=&soil;
        for(const auto& layer:t.foliage){auto& v=density_views[int((layer.offset==4?100:200)+layer.type)];v.grid=&fruit;v.offset=layer.offset;v.type=layer.type;v.mask.minimum=0;}
        bind("worldCropInfo",safe<crop_info>);bind("worldCropLayers",safe<crop_layers>);bind("setDensityCompareParams",safe<density_compare>);bind("setDensityMaskParams",safe<density_mask>);bind("setDensityReturnValueShift",safe<density_shift>);bind("setDensityTypeIndexCompareMode",safe<type_compare>);bind("setDensityNewTypeIndexMode",safe<new_type>);bind_area("getDensityParallelogram",false,false);bind_area("setDensityParallelogram",true,false);bind_area("setDensityMaskedParallelogram",true,true);
        lua_pushinteger(lua,difficulty);lua_setglobal(lua,"worldDifficulty");lua_pushstring(lua,t.map_id.c_str());lua_setglobal(lua,"worldMap");
        try{execute(resolve("src/engine/mission.lua"));fs15world::update_visibility(const_cast<fs15world::Terrain&>(terrain));resources.assign(used.begin(),used.end());}catch(...){lua_close(lua);lua=nullptr;throw;}
    }
    ~Impl(){if(lua)lua_close(lua);}
};
MissionRuntime::MissionRuntime(const fs::path& r,const fs15world::Terrain& t,int difficulty,bool reference):impl(std::make_unique<Impl>(r,t,difficulty,reference)){}
MissionRuntime::~MissionRuntime()=default;
void MissionRuntime::initialize_graphics(ID3D11Device* d,ID3D11DeviceContext* c){for(auto& b:impl->bodies)b.renderer=std::make_unique<SceneRenderer>(d,c,b.scene,impl->root);impl->foliage=std::make_unique<FoliageRenderer>(d,c,impl->terrain,impl->fruit,impl->root);auto resources=impl->foliage->resources();impl->resources.insert(impl->resources.end(),resources.begin(),resources.end());}
void MissionRuntime::update(double dt,float throttle,float steering,bool handbrake){
    const auto steps=impl->clock.advance(dt);auto l=impl->lua;
    for(unsigned i=0;i<steps;++i){lua_getglobal(l,"missionUpdate");lua_pushnumber(l,1.0f/60);lua_pushnumber(l,throttle);lua_pushnumber(l,steering);lua_pushboolean(l,handbrake);impl->call("missionUpdate",4);}
    for(auto& b:impl->bodies)if(b.renderer)b.renderer->update_instances(b.scene);
}
void MissionRuntime::set_paused(bool value){impl->clock.set_paused(value);}
bool MissionRuntime::paused()const{return impl->clock.paused();}
void MissionRuntime::action(const char* action){
    if(std::string(action)=="pause"){set_paused(!paused());return;}
    if(paused())return;
    auto l=impl->lua;lua_getglobal(l,"missionAction");lua_pushstring(l,action);impl->call("missionAction",1);if(std::string(action)=="scenario-tip"){impl->orbit_yaw=1.0f;impl->camera_distance=16;}
}
void MissionRuntime::draw(const XMMATRIX& vp,const XMFLOAT3& eye,bool shadow,bool measure){for(auto& b:impl->bodies)if(b.renderer)b.renderer->draw(vp,eye,measure,shadow);if(impl->foliage){impl->foliage->update(eye,impl->revision!=impl->rendered_revision);impl->rendered_revision=impl->revision;impl->foliage->draw(vp,eye,shadow);}}
bool MissionRuntime::camera(XMFLOAT3& eye,XMFLOAT3& target)const{if(impl->controlled<=0)return false;const auto& b=impl->bodies.at(impl->controlled-1);target={b.position.x,b.position.y+1.6f,b.position.z};float yaw=b.yaw+impl->orbit_yaw,pitch=impl->orbit_pitch,distance=impl->camera_distance;XMFLOAT3 desired{target.x-std::sin(yaw)*std::cos(pitch)*distance,target.y+std::sin(pitch)*distance,target.z-std::cos(yaw)*std::cos(pitch)*distance};eye=target;
    for(int i=1;i<=40;++i){float t=i/40.0f;XMFLOAT3 p{target.x+(desired.x-target.x)*t,target.y+(desired.y-target.y)*t,target.z+(desired.z-target.z)*t};p.y=std::max(p.y,fs15world::terrain_height(impl->terrain,p.x,p.z)+0.4f);if(impl->collision->blocked(p.x,p.z,p.y-1.3f,0.2f))break;eye=p;}if(std::hypot(eye.x-target.x,eye.z-target.z)<0.2f){eye=desired;eye.y=std::max(eye.y,target.y+1);}return true;}
void MissionRuntime::orbit(float yaw,float pitch){impl->orbit_yaw+=yaw;impl->orbit_pitch=std::clamp(impl->orbit_pitch+pitch,0.05f,1.25f);}
void MissionRuntime::zoom(float amount){impl->camera_distance=std::clamp(impl->camera_distance+amount,4.0f,35.0f);}
std::wstring MissionRuntime::status()const{auto l=impl->lua;lua_getglobal(l,"missionStatus");impl->call("missionStatus",0,1);std::string s=lua_tostring(l,-1);lua_pop(l,1);if(paused())s="PAUSED - P to resume\n"+s;return std::wstring(s.begin(),s.end());}
std::string MissionRuntime::report()const{auto l=impl->lua;lua_getglobal(l,"missionReport");impl->call("missionReport",0,1);std::string s=lua_tostring(l,-1);lua_pop(l,1);s.pop_back();s+=",\"collision_triangles\":"+std::to_string(impl->collision->triangle_count())+",\"foliage_instances\":"+std::to_string(impl->foliage?impl->foliage->instances():0)+",\"density_width\":"+std::to_string(impl->fruit.width)+",\"density_revision\":"+std::to_string(impl->revision)+",\"simulation_ticks\":"+std::to_string(impl->clock.ticks())+",\"discarded_time_s\":"+std::to_string(impl->clock.discarded_seconds())+",\"paused\":"+(paused()?"true":"false")+",\"reference_bytecode_chunks\":"+std::to_string(impl->reference_chunks)+"}";return s;}
std::uint64_t MissionRuntime::visible_samples(){std::uint64_t n=0;for(auto& b:impl->bodies)if(b.renderer)n+=b.renderer->visible_samples(false);return n;}
unsigned MissionRuntime::vehicle_count()const{return unsigned(impl->bodies.size());}
const std::vector<fs::path>& MissionRuntime::resources()const{return impl->resources;}
