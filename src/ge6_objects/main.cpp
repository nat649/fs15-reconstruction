// Authored SDL/OpenGL scene transport around recovered GE6 TRS, buffers and draw.
// Original geometry/textures are local inputs. Fixed-function lighting and LOD0
// are temporary preview bindings, not recovered GE6 shaders/resource loading.
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_opengl.h>
#include "gl_buffers.h"
#include "gl_draw.h"
#include "scene_trs.h"
#include "scene_math.h"
#include "scene_projection.h"
#include "scene_visibility.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr float pi=3.14159265358979323846f;
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
struct Input {
    std::ifstream file;
    explicit Input(const std::filesystem::path& path):file(path,std::ios::binary){require(bool(file),"Cannot open local scene resource");}
    void read(void* out,size_t size){file.read(static_cast<char*>(out),static_cast<std::streamsize>(size));require(bool(file),"Truncated scene resource");}
    template<class T>T get(){T out{};read(&out,sizeof(out));return out;}
    uint32_t count(uint32_t maximum){auto n=get<uint32_t>();require(n<=maximum,"Scene resource exceeds limit");return n;}
    std::string string(){auto n=count(1024);std::string out(n,'\0');read(out.data(),n);return out;}
    void end(){require(file.peek()==std::char_traits<char>::eof(),"Trailing resource data");}
};
template<class T>T symbol(const char* name){auto p=SDL_GL_GetProcAddress(name);require(p!=nullptr,name);return reinterpret_cast<T>(p);}
struct Driver {
    PFNGLGENBUFFERSPROC gen=symbol<PFNGLGENBUFFERSPROC>("glGenBuffers");
    PFNGLBINDBUFFERPROC bind=symbol<PFNGLBINDBUFFERPROC>("glBindBuffer");
    PFNGLBUFFERDATAPROC data=symbol<PFNGLBUFFERDATAPROC>("glBufferData");
    PFNGLDELETEBUFFERSPROC erase=symbol<PFNGLDELETEBUFFERSPROC>("glDeleteBuffers");
    using Range=void(APIENTRY*)(GLenum,GLuint,GLuint,GLsizei,GLenum,const void*);
    Range range=symbol<Range>("glDrawRangeElements");
    FS15GLBufferServices buffers{};
    FS15GLDrawServices draw{};
    alignas(16) std::array<uint8_t,0x190> state{};
    Driver(){
        buffers.user=this;
        buffers.gen=[](void* p,int n,uint32_t* b){static_cast<Driver*>(p)->gen(n,b);};
        buffers.bind=[](void* p,uint32_t t,uint32_t b){static_cast<Driver*>(p)->bind(t,b);};
        buffers.data=[](void* p,uint32_t t,size_t n,uintptr_t d,uint32_t u){static_cast<Driver*>(p)->data(t,static_cast<GLsizeiptr>(n),reinterpret_cast<void*>(d),u);};
        buffers.erase=[](void* p,int n,const uint32_t* b){static_cast<Driver*>(p)->erase(n,b);};
        buffers.allocate=[](void*,size_t n){return std::malloc(n);};buffers.release=[](void*,void* p){std::free(p);};
        draw.user=this;draw.point_size=[](void*,float s){glPointSize(s);};
        draw.arrays=[](void*,uint32_t m,int32_t first,int32_t n){glDrawArrays(m,first,n);};
        draw.elements=[](void*,uint32_t m,int32_t n,uint32_t t,uintptr_t o){glDrawElements(m,n,t,reinterpret_cast<const void*>(o));};
        draw.range=[](void* p,uint32_t m,uint32_t a,uint32_t b,int32_t n,uint32_t t,uintptr_t o){static_cast<Driver*>(p)->range(m,a,b,n,t,reinterpret_cast<const void*>(o));};
    }
};
struct Vertex {float p[3],n[3],uv[2];};
static_assert(sizeof(Vertex)==32);
struct Subset {uint32_t first,count;};
struct Mesh {
    FS15GLVertexBuffer* vertex=nullptr;FS15GLIndexBuffer index{};
    uint32_t vertex_count=0;std::vector<Subset> subsets;
    std::array<float,3> center{};float radius=0;
};
struct Material {std::array<float,4> color{};bool alpha=false;GLuint texture=0;};
struct Object {uint32_t mesh;std::array<float,16> world;std::vector<uint32_t> materials;std::array<float,3> center;float radius,clip;};
struct Scene {
    Driver& driver;std::map<uint32_t,Mesh> meshes;std::map<uint32_t,Material> materials;std::vector<Object> objects;
    std::map<std::string,GLuint> textures;std::array<float,5> camera{};
    explicit Scene(Driver& d):driver(d){}
    ~Scene(){for(auto& [id,m]:meshes){(void)id;if(m.vertex)fs15_gl_vertex_destroy(m.vertex,1,&driver.buffers);if(m.index.name)fs15_gl_index_destroy(&m.index,0,&driver.buffers);}for(auto& [name,id]:textures){(void)name;glDeleteTextures(1,&id);}}
    GLuint texture(const std::filesystem::path& folder,const std::string& name){
        if(name.empty())return 0;
        require(name.find('/')==std::string::npos&&name.find('\\')==std::string::npos&&name.find("..") == std::string::npos,"Invalid local texture name");
        auto found=textures.find(name);if(found!=textures.end())return found->second;
        Input input(folder/name);auto w=input.count(8192),h=input.count(8192);require(w&&h,"Empty texture");
        std::vector<uint8_t> pixels(size_t(w)*h*4);input.read(pixels.data(),pixels.size());input.end();
        GLuint id=0;glGenTextures(1,&id);textures.emplace(name,id);glBindTexture(GL_TEXTURE_2D,id);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,static_cast<GLsizei>(w),static_cast<GLsizei>(h),0,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());return id;
    }
    void load_meshes(const std::filesystem::path& path){
        Input input(path);std::array<char,8> magic{};input.read(magic.data(),8);require(!std::memcmp(magic.data(),"FS15MSH\1",8),"Wrong mesh cache version");
        auto count=input.count(10000);
        for(uint32_t k=0;k<count;++k){
            auto id=input.get<uint32_t>(),nv=input.count(4000000),ni=input.count(12000000),ns=input.count(4096);
            require(nv&&ni&&ni%3==0,"Invalid mesh dimensions");require(meshes.find(id)==meshes.end(),"Duplicate mesh ID");
            auto& m=meshes[id];m.vertex_count=nv;m.subsets.resize(ns);
            for(auto& sub:m.subsets){sub.first=input.get<uint32_t>();sub.count=input.get<uint32_t>();require(sub.first<=ni&&sub.count<=ni-sub.first&&sub.count%3==0,"Invalid mesh subset");}
            if(!ns)m.subsets.push_back({0,ni});
            std::vector<Vertex> vertices(nv);std::vector<uint32_t> indices(ni);input.read(vertices.data(),vertices.size()*sizeof(Vertex));input.read(indices.data(),indices.size()*4);
            std::array<float,3> lo{vertices[0].p[0],vertices[0].p[1],vertices[0].p[2]};auto hi=lo;
            for(const auto& v:vertices){for(float x:v.p)require(std::isfinite(x),"Nonfinite position");for(float x:v.n)require(std::isfinite(x),"Nonfinite normal");for(float x:v.uv)require(std::isfinite(x),"Nonfinite UV");for(int j=0;j<3;++j){lo[j]=std::min(lo[j],v.p[j]);hi[j]=std::max(hi[j],v.p[j]);}}
            for(auto index:indices)require(index<nv,"Mesh index out of bounds");
            for(int j=0;j<3;++j)m.center[j]=(lo[j]+hi[j])*.5f;
            for(const auto& v:vertices){float r=0;for(int j=0;j<3;++j)r+=(v.p[j]-m.center[j])*(v.p[j]-m.center[j]);m.radius=std::max(m.radius,std::sqrt(r));}
            FS15GLBufferDesc vd{reinterpret_cast<uintptr_t>(vertices.data()),nv*uint32_t(sizeof(Vertex)),0,0};
            FS15GLBufferDesc ib{reinterpret_cast<uintptr_t>(indices.data()),ni*4,0,1};
            m.vertex=fs15_gl_vertex_create(driver.state.data(),driver.state.size(),&vd,&driver.buffers);
            require(m.vertex&&fs15_gl_index_construct(&m.index,&ib,&driver.buffers),"GE6 mesh buffer creation failed");
        }input.end();
    }
    void load(const std::filesystem::path& path){
        load_meshes(path.parent_path()/"scene.meshes");
        Input input(path);std::array<char,8> magic{};input.read(magic.data(),8);require(!std::memcmp(magic.data(),"GE6OBJ01",8),"Wrong scene adapter version");
        auto nm=input.count(10000),nn=input.count(500000);input.read(camera.data(),sizeof(camera));for(float x:camera)require(std::isfinite(x),"Invalid scene camera");
        for(uint32_t k=0;k<nm;++k){auto id=input.get<uint32_t>();Material m;input.read(m.color.data(),sizeof(m.color));for(float x:m.color)require(std::isfinite(x),"Invalid material color");m.alpha=input.count(1)!=0;m.texture=texture(path.parent_path(),input.string());require(materials.emplace(id,m).second,"Duplicate material");}
        std::vector<std::array<float,16>> worlds;std::vector<bool> visible;worlds.reserve(nn);visible.reserve(nn);
        for(uint32_t k=0;k<nn;++k){
            auto parent=input.get<int32_t>();require(parent>=-1&&parent<static_cast<int32_t>(k),"Invalid scene hierarchy");
            auto mesh=input.get<uint32_t>(),flags=input.count(1);std::array<float,10> trs{};input.read(trs.data(),sizeof(trs));for(float x:trs)require(std::isfinite(x),"Nonfinite scene transform");require(trs[9]>=0,"Negative clip distance");
            auto ni=input.count(4096);std::vector<uint32_t> ids(ni);for(auto& id:ids){id=input.get<uint32_t>();require(materials.find(id)!=materials.end(),"Missing material ID");}
            std::array<float,16> local{},world{};
            require(fs15_compose_trs(trs.data()+3,trs.data()+6,trs.data(),local.data(),16)!=0,"GE6 TRS composition failed");
            if(parent>=0)require(fs15_affine_multiply(worlds[parent].data(),16,local.data(),16,world.data(),16)!=0,"GE6 hierarchy composition failed");else world=local;
            bool shown=flags&&(parent<0||visible[parent]);worlds.push_back(world);visible.push_back(shown);
            auto found=meshes.find(mesh);if(!shown||!ni||found==meshes.end())continue;
            const auto& m=found->second;require(ni==m.subsets.size(),"Material/subset count mismatch");Object object{mesh,world,std::move(ids),{},0,trs[9]};
            float norm=0;for(int j=0;j<3;++j){object.center[j]=world[12+j];for(int a=0;a<3;++a){object.center[j]+=world[a*4+j]*m.center[a];norm+=world[a*4+j]*world[a*4+j];}}
            object.radius=m.radius*std::sqrt(norm);objects.push_back(std::move(object));
        }input.end();require(!objects.empty(),"No renderable scene objects");
        std::cout<<"GE6 object transport: "<<meshes.size()<<" decoded meshes, "<<objects.size()<<" eligible instances, "<<textures.size()<<" diffuse textures\n";
    }
};
struct Options {std::filesystem::path scene,capture;int frames=0;float distance=600;std::array<float,5> camera{};bool override_camera=false;};
Options options(int argc,char** argv){Options o;for(int i=1;i<argc;++i){std::string a=argv[i];auto next=[&](){require(i+1<argc,"Missing option value");return argv[++i];};
    if(a=="--scene")o.scene=next();else if(a=="--capture")o.capture=next();else if(a=="--frames")o.frames=std::stoi(next());else if(a=="--distance")o.distance=std::stof(next());else if(a=="--camera"){for(auto& x:o.camera)x=std::stof(next());o.override_camera=true;}else throw std::runtime_error("Unknown option: "+a);}
    require(!o.scene.empty(),"Usage: FS15GE6Objects --scene scene.ge6scene [--capture image.bmp --frames 1] [--camera x y z yaw pitch]");require(o.frames>=0&&std::isfinite(o.distance)&&o.distance>0&&o.distance<=10000,"Invalid frame/distance option");for(float x:o.camera)require(std::isfinite(x),"Invalid camera option");return o;}

int run(SDL_Window* window,const Options& o){
    Driver driver;Scene scene(driver);scene.load(o.scene);auto camera=o.override_camera?o.camera:scene.camera;
    std::cout<<"GL renderer: "<<glGetString(GL_RENDERER)<<"\nWASD/arrows or ZQSD: fly; Space/Ctrl: height; Shift: faster; right mouse: look; Escape: exit\n";
    glEnable(GL_DEPTH_TEST);glEnable(GL_NORMALIZE);glEnable(GL_LIGHTING);glEnable(GL_LIGHT0);glEnable(GL_COLOR_MATERIAL);glColorMaterial(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE);
    const GLfloat ambient[]{.55f,.55f,.55f,1},diffuse[]{.7f,.7f,.65f,1};glLightModelfv(GL_LIGHT_MODEL_AMBIENT,ambient);glLightfv(GL_LIGHT0,GL_DIFFUSE,diffuse);glLightModeli(GL_LIGHT_MODEL_TWO_SIDE,GL_TRUE);
    glEnableClientState(GL_VERTEX_ARRAY);glEnableClientState(GL_NORMAL_ARRAY);glEnableClientState(GL_TEXTURE_COORD_ARRAY);glClearColor(.48f,.67f,.84f,1);
    FS15GLDrawState state{};state.layout=1;bool running=true;int frame=0;uint64_t previous=SDL_GetPerformanceCounter();size_t submitted=0,triangles=0;
    while(running){
        auto now=SDL_GetPerformanceCounter();float dt=std::min(.05f,float(double(now-previous)/double(SDL_GetPerformanceFrequency())));previous=now;
        SDL_Event event;while(SDL_PollEvent(&event)){if(event.type==SDL_QUIT)running=false;if(event.type==SDL_KEYDOWN&&event.key.keysym.sym==SDLK_ESCAPE)running=false;
            if(event.type==SDL_MOUSEMOTION&&(event.motion.state&SDL_BUTTON_RMASK)){camera[3]-=event.motion.xrel*.15f;camera[4]=std::clamp(camera[4]-event.motion.yrel*.15f,-89.f,89.f);}}
        auto keys=SDL_GetKeyboardState(nullptr);float speed=dt*(keys[SDL_SCANCODE_LSHIFT]?70.f:15.f),yaw=camera[3]*pi/180,pitch=camera[4]*pi/180;
        float forward=float(keys[SDL_SCANCODE_W]||keys[SDL_SCANCODE_Z]||keys[SDL_SCANCODE_UP])-float(keys[SDL_SCANCODE_S]||keys[SDL_SCANCODE_DOWN]);
        float right=float(keys[SDL_SCANCODE_D]||keys[SDL_SCANCODE_RIGHT])-float(keys[SDL_SCANCODE_A]||keys[SDL_SCANCODE_Q]||keys[SDL_SCANCODE_LEFT]);
        camera[0]+=speed*(-std::sin(yaw)*std::cos(pitch)*forward+std::cos(yaw)*right);camera[2]+=speed*(-std::cos(yaw)*std::cos(pitch)*forward-std::sin(yaw)*right);
        camera[1]+=speed*(std::sin(pitch)*forward+float(keys[SDL_SCANCODE_SPACE])-float(keys[SDL_SCANCODE_LCTRL]));
        int width,height;SDL_GL_GetDrawableSize(window,&width,&height);if(width<=0||height<=0){SDL_Delay(10);continue;}
        glViewport(0,0,width,height);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        float top=.1f*std::tan(60*pi/360),side=top*float(width)/float(height);
        std::array<float,16> projection{},camera_world{},view{},combined{};const float rotation[]{pitch,yaw,0},scale[]{1,1,1};
        require(fs15_scene_perspective(-side,side,-top,top,.1f,10000,projection.data(),16)!=0,"GE6 perspective failed");
        require(fs15_compose_trs(rotation,scale,camera.data(),camera_world.data(),16)&&fs15_affine_inverse(camera_world.data(),16,view.data(),16),"GE6 camera transform failed");
        require(fs15_scene_projective_multiply(projection.data(),16,view.data(),16,combined.data(),16)!=0,"GE6 view projection failed");
        FS15SceneFrustum frustum{};require(fs15_scene_frustum_matrix(&frustum,combined.data(),16)!=0,"GE6 frustum failed");
        glMatrixMode(GL_PROJECTION);glLoadMatrixf(projection.data());glMatrixMode(GL_MODELVIEW);glLoadMatrixf(view.data());const GLfloat light[]{-.4f,1,.3f,0};glLightfv(GL_LIGHT0,GL_POSITION,light);
        submitted=triangles=0;
        for(const auto& object:scene.objects){float dist2=0;for(int j=0;j<3;++j)dist2+=(object.center[j]-camera[j])*(object.center[j]-camera[j]);float limit=std::min(o.distance,object.clip)+object.radius;if(dist2>limit*limit)continue;
            const float sphere[]{object.center[0],object.center[1],object.center[2],object.radius};int visible=0;require(fs15_scene_frustum_sphere(&frustum,sphere,&visible)!=0,"GE6 object visibility query failed");if(!visible)continue;
            const auto& mesh=scene.meshes.at(object.mesh);driver.bind(GL_ARRAY_BUFFER,mesh.vertex->name);driver.bind(GL_ELEMENT_ARRAY_BUFFER,mesh.index.name);
            glVertexPointer(3,GL_FLOAT,sizeof(Vertex),nullptr);glNormalPointer(GL_FLOAT,sizeof(Vertex),reinterpret_cast<void*>(12));glTexCoordPointer(2,GL_FLOAT,sizeof(Vertex),reinterpret_cast<void*>(24));
            glPushMatrix();glMultMatrixf(object.world.data());state.index=const_cast<FS15GLIndexBuffer*>(&mesh.index);
            for(size_t k=0;k<mesh.subsets.size();++k){const auto& material=scene.materials.at(object.materials[k]);const auto& sub=mesh.subsets[k];
                glColor4fv(material.color.data());if(material.texture){glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,material.texture);}else glDisable(GL_TEXTURE_2D);
                if(material.alpha){glEnable(GL_ALPHA_TEST);glAlphaFunc(GL_GREATER,.4f);}else glDisable(GL_ALPHA_TEST);
                require(fs15_gl_draw(&state,sub.count,sub.first,0,0,mesh.vertex_count,&driver.draw)!=0,"GE6 subset draw failed");triangles+=sub.count/3;
            }glPopMatrix();++submitted;
        }
        require(glGetError()==GL_NO_ERROR,"OpenGL object rendering error");++frame;
        if(!o.capture.empty()&&(o.frames==0||frame==o.frames)){
            auto* surface=SDL_CreateRGBSurfaceWithFormat(0,width,height,24,SDL_PIXELFORMAT_RGB24);require(surface!=nullptr,"Capture allocation failed");glPixelStorei(GL_PACK_ALIGNMENT,1);
            std::vector<uint8_t> pixels(size_t(width)*height*3);glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
            for(int y=0;y<height;++y)std::memcpy(static_cast<uint8_t*>(surface->pixels)+y*surface->pitch,pixels.data()+size_t(height-1-y)*width*3,size_t(width)*3);
            int result=SDL_SaveBMP(surface,o.capture.string().c_str());SDL_FreeSurface(surface);require(result==0,"Cannot save capture");if(!o.frames)running=false;
        }
        SDL_GL_SwapWindow(window);if(o.frames&&frame>=o.frames)running=false;
        if(frame%30==0){std::string title="GE6 recovered draw | "+std::to_string(submitted)+" original objects | temporary materials";SDL_SetWindowTitle(window,title.c_str());}
    }
    std::cout<<"Submitted "<<submitted<<" object instances / "<<triangles<<" triangles in final frame; temporary lighting/LOD/material bindings\n";
    return 0;
}
}
int main(int argc,char** argv){
    SDL_Window* window=nullptr;SDL_GLContext context=nullptr;int result=1;
    try{auto o=options(argc,argv);SDL_SetMainReady();require(SDL_Init(SDL_INIT_VIDEO)==0,SDL_GetError());SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,24);SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
        window=SDL_CreateWindow("GE6 original objects - temporary material transport",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,1280,720,SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE|(o.frames?SDL_WINDOW_HIDDEN:SDL_WINDOW_SHOWN));require(window!=nullptr,SDL_GetError());context=SDL_GL_CreateContext(window);require(context!=nullptr,SDL_GetError());SDL_GL_SetSwapInterval(o.frames?0:1);result=run(window,o);
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';}
    if(context)SDL_GL_DeleteContext(context);
    if(window)SDL_DestroyWindow(window);
    SDL_Quit();return result;
}
