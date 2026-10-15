// Original crop block meshes/materials and density states. The cell sampler and
// deterministic jitter below are host approximations of GE6 foliage placement.
#include "foliage.h"
#include <algorithm>
#include <map>
#include <set>
#include <cmath>
using namespace DirectX;
FoliageRenderer::FoliageRenderer(ID3D11Device* d,ID3D11DeviceContext* c,const fs15world::Terrain& t,const fs15world::DensityGrid& g,const std::filesystem::path& root):terrain(t),grid(g){
    std::map<unsigned,unsigned> meshes;for(const auto& mesh:t.meshes)for(const auto& layer:t.foliage)if(std::find(layer.shapes.begin(),layer.shapes.end(),mesh.id)!=layer.shapes.end()&&!meshes.count(mesh.id)){meshes[mesh.id]=unsigned(scene.meshes.size());scene.meshes.push_back(mesh);}
    std::set<std::filesystem::path> used;
    for(unsigned l=0;l<t.foliage.size();++l){const auto& layer=t.foliage[l];
        for(unsigned state=0;state<layer.states;++state)if(!layer.shapes.empty()&&meshes.count(layer.shapes[std::min(size_t(state),layer.shapes.size()-1)])){
            auto mat=t.materials.at(layer.material);scene.materials[layer.material]=mat;if(!mat.texture.empty())used.insert(mat.texture);
            fs15world::Matrix identity{};XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(identity.value),XMMatrixIdentity());
            auto mesh=meshes.at(layer.shapes[std::min(size_t(state),layer.shapes.size()-1)]);std::vector<int> materials(scene.meshes[mesh].subsets.size(),layer.material);
            scene.groups.push_back({mesh,materials,{identity},false,true,{}});types.push_back({l,state});
        }
    }
    scene.used_resources.assign(used.begin(),used.end());renderer=std::make_unique<SceneRenderer>(d,c,scene,root);
}
void FoliageRenderer::update(const XMFLOAT3& eye,bool force){
    int cell_x=int(std::floor(eye.x)),cell_z=int(std::floor(eye.z));if(!force&&cell_x==last_x&&cell_z==last_z)return;last_x=cell_x;last_z=cell_z;
    for(auto& g:scene.groups)g.instances.clear();count=0;
    std::map<std::pair<unsigned,unsigned>,unsigned> groups;for(unsigned i=0;i<types.size();++i)groups[{types[i].layer,types[i].state}]=i;
    auto hash=[](unsigned x){x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;x^=x>>16;return x;};
    // A fixed one-metre candidate lattice bounds upload/draw cost. Never fill
    // empty density cells or synthesize a different crop/state.
    for(int z=cell_z-80;z<=cell_z+80;++z)for(int x=cell_x-80;x<=cell_x+80;++x){
        if((x-eye.x)*(x-eye.x)+(z-eye.z)*(z-eye.z)>6400)continue;
        int px=int((x/terrain.width+0.5f)*grid.width),pz=int((z/terrain.depth+0.5f)*grid.height);
        if(px<0||pz<0||px>=int(grid.width)||pz>=int(grid.height))continue;
        auto value=grid.values[size_t(pz)*grid.width+px];if(!(value&15))continue;
        for(unsigned l=0;l<terrain.foliage.size();++l){const auto& layer=terrain.foliage[l];if(layer.type!=unsigned(value&15))continue;
            unsigned encoded=(value>>layer.offset)&((1u<<layer.channels)-1);if(!encoded)continue;unsigned state=encoded-1;auto group=groups.find({l,state});if(group==groups.end())continue;
            auto select=[&](const std::vector<float>& v){return v.empty()?1:v[std::min(size_t(state),v.size()-1)];};float w=select(layer.widths),h=select(layer.heights);if(w<=0||h<=0)continue;
            unsigned seed=hash(unsigned(x)*73856093u^unsigned(z)*19349663u);float xx=x+(float(seed&255)/255-0.5f)*0.6f,zz=z+(float((seed>>8)&255)/255-0.5f)*0.6f;
            XMFLOAT4X4 world;XMStoreFloat4x4(&world,XMMatrixScaling(w,h,w)*XMMatrixRotationY(float(seed>>16)/65535*XM_2PI)*XMMatrixTranslation(xx,fs15world::terrain_height(terrain,xx,zz),zz));fs15world::Matrix m;std::copy(&world._11,&world._11+16,m.value);scene.groups[group->second].instances.push_back(m);++count;
        }
    }
    renderer->update_instances(scene);
}
void FoliageRenderer::draw(const XMMATRIX& vp,const XMFLOAT3& eye,bool shadow){if(!shadow)renderer->draw(vp,eye);}
