#pragma once
#include "resources.h"
#include "density.h"
#include "scene_render.h"
#include <memory>
#include <climits>
class FoliageRenderer {
    fs15world::Terrain scene;
    const fs15world::Terrain& terrain;
    const fs15world::DensityGrid& grid;
    std::unique_ptr<SceneRenderer> renderer;
    struct Type {unsigned layer,state;};std::vector<Type> types;
    int last_x=INT_MIN,last_z=INT_MIN;
    unsigned count=0;
public:
    FoliageRenderer(ID3D11Device*,ID3D11DeviceContext*,const fs15world::Terrain&,const fs15world::DensityGrid&,const std::filesystem::path&);
    void update(const DirectX::XMFLOAT3&,bool force=false);
    void draw(const DirectX::XMMATRIX&,const DirectX::XMFLOAT3&,bool shadow=false);
    unsigned instances()const{return count;}
    const std::vector<std::filesystem::path>& resources()const{return scene.used_resources;}
};
