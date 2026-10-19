/* Authored context/GL symbol transport; buffer algorithms are recovered core.
 * Uses no game resources. This checks GPU data, not a rendered game scene. */
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_opengl.h>
#include "gl_buffers.h"
#include "gl_draw.h"
#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
template<class T>T symbol(const char* name){auto p=SDL_GL_GetProcAddress(name);if(!p)throw std::runtime_error(name);return reinterpret_cast<T>(p);}
struct GL {
    PFNGLGENBUFFERSPROC gen=symbol<PFNGLGENBUFFERSPROC>("glGenBuffers");
    PFNGLBINDBUFFERPROC bind=symbol<PFNGLBINDBUFFERPROC>("glBindBuffer");
    PFNGLBUFFERDATAPROC data=symbol<PFNGLBUFFERDATAPROC>("glBufferData");
    PFNGLBUFFERSUBDATAPROC sub=symbol<PFNGLBUFFERSUBDATAPROC>("glBufferSubData");
    PFNGLMAPBUFFERPROC map=symbol<PFNGLMAPBUFFERPROC>("glMapBuffer");
    PFNGLUNMAPBUFFERPROC unmap=symbol<PFNGLUNMAPBUFFERPROC>("glUnmapBuffer");
    PFNGLDELETEBUFFERSPROC erase=symbol<PFNGLDELETEBUFFERSPROC>("glDeleteBuffers");
    PFNGLISBUFFERPROC exists=symbol<PFNGLISBUFFERPROC>("glIsBuffer");
    using Error=GLenum (APIENTRY*)();Error error=symbol<Error>("glGetError");
    using String=const GLubyte* (APIENTRY*)(GLenum);String string=symbol<String>("glGetString");
    static GL& self(void* p){return *static_cast<GL*>(p);}
    bool unmapped=true;
    FS15GLBufferServices services(void* scratch,size_t size){FS15GLBufferServices s{};s.user=this;
        s.gen=[](void* p,int n,uint32_t* b){self(p).gen(n,b);};s.bind=[](void* p,uint32_t t,uint32_t b){self(p).bind(t,b);};
        s.data=[](void* p,uint32_t t,size_t n,uintptr_t d,uint32_t u){self(p).data(t,static_cast<GLsizeiptr>(n),reinterpret_cast<void*>(d),u);};
        s.subdata=[](void* p,uint32_t t,size_t o,size_t n,uintptr_t d){self(p).sub(t,static_cast<GLintptr>(o),static_cast<GLsizeiptr>(n),reinterpret_cast<void*>(d));};
        s.map=[](void* p,uint32_t t,uint32_t a){return reinterpret_cast<uintptr_t>(self(p).map(t,a));};
        s.unmap=[](void* p,uint32_t t){auto& g=self(p);g.unmapped=g.unmap(t)==GL_TRUE&&g.unmapped;};
        s.erase=[](void* p,int n,const uint32_t* b){self(p).erase(n,b);};
        s.allocate=[](void*,size_t n){return std::malloc(n);};s.release=[](void*,void* p){std::free(p);};
        s.scratch=reinterpret_cast<uintptr_t>(scratch);s.scratch_bytes=size;return s;
    }
};
void require(bool v,const char* why){if(!v)throw std::runtime_error(why);}
void draw_check(GL& g,FS15GLBufferServices& buffers,void* driver,size_t driver_size){
    // Authored fixed-function context/attribute transport for a primitive
    // readback check. This does not stand in for recovered GE6 scene shaders.
    using Arrays=void(APIENTRY*)(GLenum,GLint,GLsizei);using Elements=void(APIENTRY*)(GLenum,GLsizei,GLenum,const void*);
    using Range=void(APIENTRY*)(GLenum,GLuint,GLuint,GLsizei,GLenum,const void*);using Point=void(APIENTRY*)(GLfloat);
    struct Draw {Arrays arrays;Elements elements;Range range;Point point;};
    Draw draw{symbol<Arrays>("glDrawArrays"),symbol<Elements>("glDrawElements"),symbol<Range>("glDrawRangeElements"),symbol<Point>("glPointSize")};
    FS15GLDrawServices s{};s.user=&draw;s.point_size=[](void* p,float n){static_cast<Draw*>(p)->point(n);};
    s.arrays=[](void* p,uint32_t m,int32_t f,int32_t n){static_cast<Draw*>(p)->arrays(m,f,n);};
    s.elements=[](void* p,uint32_t m,int32_t n,uint32_t t,uintptr_t o){static_cast<Draw*>(p)->elements(m,n,t,reinterpret_cast<const void*>(o));};
    s.range=[](void* p,uint32_t m,uint32_t a,uint32_t b,int32_t n,uint32_t t,uintptr_t o){static_cast<Draw*>(p)->range(m,a,b,n,t,reinterpret_cast<const void*>(o));};
    const float triangle[]{-.8f,-.8f,0,.8f,-.8f,0,0,.8f,0};const uint16_t ids[]{0,1,2};
    FS15GLBufferDesc vd{reinterpret_cast<uintptr_t>(triangle),sizeof(triangle),4,0},id{reinterpret_cast<uintptr_t>(ids),sizeof(ids),4,0};
    auto* vertex=fs15_gl_vertex_create(driver,driver_size,&vd,&buffers);FS15GLIndexBuffer index{};require(vertex&&fs15_gl_index_construct(&index,&id,&buffers),"draw buffer creation");
    auto viewport=symbol<void(APIENTRY*)(GLint,GLint,GLsizei,GLsizei)>("glViewport");auto matrix=symbol<void(APIENTRY*)(GLenum)>("glMatrixMode");auto identity=symbol<void(APIENTRY*)()>("glLoadIdentity");
    auto enable=symbol<void(APIENTRY*)(GLenum)>("glEnableClientState"),disable=symbol<void(APIENTRY*)(GLenum)>("glDisableClientState");auto pointer=symbol<void(APIENTRY*)(GLint,GLenum,GLsizei,const void*)>("glVertexPointer");
    auto clear_color=symbol<void(APIENTRY*)(GLfloat,GLfloat,GLfloat,GLfloat)>("glClearColor");auto clear=symbol<void(APIENTRY*)(GLbitfield)>("glClear");auto color=symbol<void(APIENTRY*)(GLfloat,GLfloat,GLfloat)>("glColor3f");
    auto read=symbol<void(APIENTRY*)(GLint,GLint,GLsizei,GLsizei,GLenum,GLenum,void*)>("glReadPixels");
    viewport(0,0,64,64);matrix(GL_PROJECTION);identity();matrix(GL_MODELVIEW);identity();enable(GL_VERTEX_ARRAY);g.bind(GL_ARRAY_BUFFER,vertex->name);pointer(3,GL_FLOAT,12,nullptr);color(0,0,1);clear_color(0,0,0,1);
    FS15GLDrawState state{};state.layout=1;
    for(int pass=0;pass<4;++pass){clear(GL_COLOR_BUFFER_BIT);state.index=pass?&index:nullptr;state.layout=pass==3?0:1;
        require(fs15_gl_draw(&state,3,0,0,0,pass==2?3:0,&s),"GE6 draw rejected");std::array<uint8_t,4> pixel{};read(32,32,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel.data());
        require(pass==3?(pixel[0]==0&&pixel[1]==0&&pixel[2]==0):(pixel[0]<20&&pixel[1]<20&&pixel[2]>200),"GE6 draw framebuffer readback differs");
    }
    disable(GL_VERTEX_ARRAY);require(fs15_gl_vertex_destroy(vertex,1,&buffers)&&fs15_gl_index_destroy(&index,0,&buffers),"draw buffer cleanup");
    require(!g.error(),"GE6 draw GL error");std::cout<<"GE6 recovered draw: arrays, elements, range and missing-layout suppression passed actual framebuffer readback\n";
}
int run(){
    GL g;std::cout<<"GL renderer: "<<g.string(GL_RENDERER)<<'\n';
    std::array<uint8_t,256> scratch{},initial{};for(size_t j=0;j<256;++j)initial[j]=static_cast<uint8_t>(j);
    auto s=g.services(scratch.data(),scratch.size());std::array<uint8_t,0x190> driver{};
    FS15GLBufferDesc d{reinterpret_cast<uintptr_t>(initial.data()),256,4,0};
    auto* v=fs15_gl_vertex_create(driver.data(),driver.size(),&d,&s);require(v&&g.exists(v->name),"vertex allocation");
    for(int mode:{0,1}){
        auto p=fs15_gl_vertex_map(v,16,32,mode,&s);require(p==s.scratch,"staging map");std::memset(reinterpret_cast<void*>(p),0x65+mode,32);
        require(fs15_gl_vertex_unmap(v,&s),"staging upload");
        p=fs15_gl_vertex_map(v,16,32,2,&s);require(p,"readback map");
        for(size_t j=0;j<32;++j)require(reinterpret_cast<uint8_t*>(p)[j]==0x65+mode,"vertex readback bytes");
        if(!mode){require(reinterpret_cast<uint8_t*>(p)[-1]==initial[15],"untouched prefix");require(reinterpret_cast<uint8_t*>(p)[32]==initial[48],"untouched suffix");}
        require(fs15_gl_vertex_unmap(v,&s),"readback unmap");
    }
    // Occupied scratch forces native's direct GL mapping path.
    s.scratch_busy=1;auto p=fs15_gl_vertex_map(v,8,8,1,&s);require(p&& !v->staged,"direct map");std::memset(reinterpret_cast<void*>(p),0x35,8);
    require(fs15_gl_vertex_unmap(v,&s),"direct unmap");s.scratch_busy=0;
    p=fs15_gl_vertex_map(v,8,8,2,&s);require(p,"direct readback");for(size_t j=0;j<8;++j)require(reinterpret_cast<uint8_t*>(p)[j]==0x35,"direct bytes");require(fs15_gl_vertex_unmap(v,&s),"direct read unmap");
    FS15GLIndexBuffer i{};require(fs15_gl_index_construct(&i,&d,&s)&&g.exists(i.name),"index allocation");
    p=fs15_gl_index_map(&i,0,2,&s);require(p&&!std::memcmp(reinterpret_cast<void*>(p),initial.data(),256),"index initial bytes");require(fs15_gl_index_unmap(&i,&s),"index read unmap");
    p=fs15_gl_index_map(&i,4,1,&s);require(p,"index discard map");std::memset(reinterpret_cast<void*>(p),0x76,16);require(fs15_gl_index_unmap(&i,&s),"index write unmap");
    p=fs15_gl_index_map(&i,4,2,&s);require(p,"index readback");for(size_t j=0;j<16;++j)require(reinterpret_cast<uint8_t*>(p)[j]==0x76,"index bytes");require(fs15_gl_index_unmap(&i,&s),"index unmap");
    auto vn=v->name,in=i.name;require(fs15_gl_vertex_destroy(v,1,&s)&&fs15_gl_index_destroy(&i,0,&s),"destruction");
    require(!g.exists(vn)&&!g.exists(in)&&!s.vertex_bytes&&!s.index_bytes&&g.unmapped&&!g.error(),"GL lifetime/error state");
    draw_check(g,s,driver.data(),driver.size());
    std::cout<<"GE6 recovered buffers and primitive draw passed; complete scene shaders/materials are not tested\n";return 0;
}
int main(){
    SDL_SetMainReady();if(SDL_Init(SDL_INIT_VIDEO)){std::cerr<<SDL_GetError()<<'\n';return 1;}
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
    auto* window=SDL_CreateWindow("GE6 buffer conformance",0,0,64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
    auto context=window?SDL_GL_CreateContext(window):nullptr;int result=1;
    if(context){try{result=run();}catch(const std::exception& e){std::cerr<<e.what()<<'\n';}}else std::cerr<<SDL_GetError()<<'\n';
    if(context)SDL_GL_DeleteContext(context);if(window)SDL_DestroyWindow(window);SDL_Quit();return result;
}
