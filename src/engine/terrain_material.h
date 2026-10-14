#pragma once
#include "resources.h"
#include <d3d11.h>
#include <memory>
class TerrainMaterial {
    struct Impl;std::unique_ptr<Impl> impl;
public:
    TerrainMaterial(ID3D11Device* device,const fs15world::Terrain& terrain,const std::filesystem::path& root);
    ~TerrainMaterial();
    void bind(ID3D11DeviceContext* context);
    unsigned layer_count()const;
};
