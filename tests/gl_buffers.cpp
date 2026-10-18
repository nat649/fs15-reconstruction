#include "gl_buffers.h"
#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#define CHECK(x) do{if(!(x)){std::cerr<<"Failed: " #x " line "<<__LINE__<<'\n';return 1;}}while(0)
struct Fixture {
    std::array<unsigned char,256> gpu{},scratch{};uint32_t name=0,target=0,access=0;int orphan=0,uploads=0,maps=0,unmaps=0,deletes=0;
    static Fixture& self(void* p){return *static_cast<Fixture*>(p);}
    FS15GLBufferServices api(){FS15GLBufferServices s{};s.user=this;
        s.gen=[](void* p,int,uint32_t* n){*n=++self(p).name;};
        s.bind=[](void* p,uint32_t t,uint32_t){self(p).target=t;};
        s.data=[](void* p,uint32_t,size_t n,uintptr_t d,uint32_t){auto& f=self(p);if(d)std::memcpy(f.gpu.data(),reinterpret_cast<void*>(d),n);else{f.gpu.fill(0);++f.orphan;}};
        s.subdata=[](void* p,uint32_t,size_t o,size_t n,uintptr_t d){auto& f=self(p);std::memcpy(f.gpu.data()+o,reinterpret_cast<void*>(d),n);++f.uploads;};
        s.map=[](void* p,uint32_t,uint32_t a){auto& f=self(p);++f.maps;f.access=a;return reinterpret_cast<uintptr_t>(f.gpu.data());};
        s.unmap=[](void* p,uint32_t){++self(p).unmaps;};s.erase=[](void* p,int,const uint32_t*){++self(p).deletes;};
        s.allocate=[](void*,size_t n){return std::malloc(n);};s.release=[](void*,void* p){std::free(p);};
        s.scratch=reinterpret_cast<uintptr_t>(scratch.data());s.scratch_bytes=scratch.size();return s;
    }
};
int main(){
    Fixture f;auto s=f.api();std::array<unsigned char,0x190> driver{};
    FS15GLBufferDesc d{0,256,4,0};auto* v=fs15_gl_vertex_create(driver.data(),driver.size(),&d,&s);
    CHECK(v&&v->usage==0x88e8&&s.vertex_bytes==256);
    auto p=fs15_gl_vertex_map(v,12,24,1,&s);CHECK(p==s.scratch&&v->staged&&s.scratch_busy&&f.maps==0);
    std::memset(reinterpret_cast<void*>(p),0x57,24);
    // A second vertex map while scratch is occupied must map the real buffer.
    FS15GLVertexBuffer other=*v;other.staged=0;
    CHECK(fs15_gl_vertex_map(&other,4,8,0,&s)==reinterpret_cast<uintptr_t>(f.gpu.data())+4);
    CHECK(fs15_gl_vertex_unmap(&other,&s)&&s.scratch_busy&&f.unmaps==1);
    CHECK(fs15_gl_vertex_unmap(v,&s)&&!s.scratch_busy&&!v->mapped&&f.uploads==1);
    CHECK(f.gpu[11]==0&&f.gpu[12]==0x57&&f.gpu[35]==0x57&&f.gpu[36]==0);
    p=fs15_gl_vertex_map(v,12,24,2,&s);CHECK(p==reinterpret_cast<uintptr_t>(f.gpu.data())+12&&f.access==0x88b8);
    CHECK(fs15_gl_vertex_unmap(v,&s));
    s.scratch_bytes=1;CHECK(!fs15_gl_vertex_map(v,0,24,0,&s)&&!v->mapped&&!s.scratch_busy);s.scratch_bytes=256;
    FS15GLIndexBuffer i{};CHECK(fs15_gl_index_construct(&i,&d,&s)&&i.type==0x1403&&s.index_bytes==256);
    p=fs15_gl_index_map(&i,8,1,&s);CHECK(p==reinterpret_cast<uintptr_t>(f.gpu.data())+8&&f.access==0x88b9);
    CHECK(fs15_gl_index_unmap(&i,&s)&&!i.mapped);
    CHECK(fs15_gl_index_destroy(&i,0,&s)&&s.index_bytes==0&&i.vtable==0x1404ba190);
    CHECK(fs15_gl_vertex_destroy(v,1,&s)&&s.vertex_bytes==0&&f.deletes==2);
    CHECK(!fs15_gl_vertex_create(driver.data(),8,&d,&s));
    std::cout<<"Recovered GL buffer staging, contention, readback, discard and lifetime passed\n";
}
