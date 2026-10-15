#include "collision.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
using namespace DirectX;
CollisionWorld::CollisionWorld(const fs15world::Terrain& scene){
    for(const auto& node:scene.nodes)if(node.collidable&&node.mesh>=0){
        const auto& mesh=scene.meshes[node.mesh];XMFLOAT4X4 world;std::copy(node.world,node.world+16,&world._11);auto m=XMLoadFloat4x4(&world);
        auto vertex=[&](unsigned index){const auto& v=mesh.vertices[index];XMFLOAT3 p;XMStoreFloat3(&p,XMVector3TransformCoord(XMVectorSet(v.x,v.y,v.z,1),m));return p;};
        for(size_t i=0;i<mesh.indices.size();i+=3){Triangle t{vertex(mesh.indices[i]),vertex(mesh.indices[i+1]),vertex(mesh.indices[i+2])};auto normal=XMVector3Cross(XMLoadFloat3(&t.b)-XMLoadFloat3(&t.a),XMLoadFloat3(&t.c)-XMLoadFloat3(&t.a));float length=XMVectorGetX(XMVector3Length(normal));
            if(length<1e-8f||std::abs(XMVectorGetY(normal))/length>0.75f)continue;
            int x0=int(std::floor(std::min({t.a.x,t.b.x,t.c.x})/16)),x1=int(std::floor(std::max({t.a.x,t.b.x,t.c.x})/16));
            int z0=int(std::floor(std::min({t.a.z,t.b.z,t.c.z})/16)),z1=int(std::floor(std::max({t.a.z,t.b.z,t.c.z})/16));
            if(x1-x0>256||z1-z0>256)throw std::runtime_error("Collision triangle extent exceeds supported map");
            unsigned index=unsigned(triangles.size());triangles.push_back(t);for(int z=z0;z<=z1;++z)for(int x=x0;x<=x1;++x)cells[{x,z}].push_back(index);
        }
    }
}
bool CollisionWorld::blocked(float x,float z,float base,float radius)const{
    if(!std::isfinite(x)||!std::isfinite(z)||!std::isfinite(base)||!std::isfinite(radius)||radius<=0||radius>5||std::abs(x)>1000000||std::abs(z)>1000000)throw std::runtime_error("Invalid static collision query");
    const int x0=int(std::floor((x-radius)/16)),x1=int(std::floor((x+radius)/16)),z0=int(std::floor((z-radius)/16)),z1=int(std::floor((z+radius)/16));
    for(int zz=z0;zz<=z1;++zz)for(int xx=x0;xx<=x1;++xx){auto cell=cells.find({xx,zz});if(cell==cells.end())continue;
        for(unsigned index:cell->second){const auto& t=triangles[index];
            for(float height:{base+0.6f,base+1.3f,base+2.0f}){
                if(height<std::min({t.a.y,t.b.y,t.c.y})||height>std::max({t.a.y,t.b.y,t.c.y}))continue;
                XMFLOAT3 vertices[]{t.a,t.b,t.c};XMFLOAT2 points[3];unsigned n=0;
                for(unsigned edge=0;edge<3;++edge){auto a=vertices[edge],b=vertices[(edge+1)%3];if((height>=a.y&&height<=b.y)||(height>=b.y&&height<=a.y)){
                    if(std::abs(b.y-a.y)>1e-8f){float f=(height-a.y)/(b.y-a.y);points[n++]={a.x+f*(b.x-a.x),a.z+f*(b.z-a.z)};}
                }}
                if(n<2)continue;auto a=points[0],b=points[1];if(n==3){float best=-1;for(unsigned i=0;i<n;++i)for(unsigned j=i+1;j<n;++j){float dx=points[j].x-points[i].x,dz=points[j].y-points[i].y;if(dx*dx+dz*dz>best){best=dx*dx+dz*dz;a=points[i];b=points[j];}}}float dx=b.x-a.x,dz=b.y-a.y,length=dx*dx+dz*dz;float f=length>1e-12f?std::clamp(((x-a.x)*dx+(z-a.y)*dz)/length,0.0f,1.0f):0;
                dx=x-a.x-f*dx;dz=z-a.y-f*dz;if(dx*dx+dz*dz<radius*radius)return true;
            }
        }
    }
    return false;
}
