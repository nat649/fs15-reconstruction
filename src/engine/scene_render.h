#pragma once
#include "resources.h"
#include <d3d11.h>
#include <DirectXMath.h>
#include <memory>

class SceneRenderer {
    struct Impl;
    std::unique_ptr<Impl> impl;
public:
    SceneRenderer(ID3D11Device* device,ID3D11DeviceContext* context,
                  const fs15world::Terrain& scene,const std::filesystem::path& root);
    ~SceneRenderer();
    void draw(const DirectX::XMMATRIX& view_projection,const DirectX::XMFLOAT3& eye,bool measure=false);
    unsigned texture_count()const;
    std::uint64_t visible_samples();
};
