#pragma once
#include "environment.h"
#include <d3d11.h>
#include <DirectXMath.h>
#include <memory>
class ShadowRenderer {
    struct Impl;std::unique_ptr<Impl> impl;
public:
    ShadowRenderer(ID3D11Device* device,const fs15world::Sun& sun);
    ~ShadowRenderer();
    DirectX::XMMATRIX update(ID3D11DeviceContext* context,const EnvironmentSample& environment,
                            const DirectX::XMFLOAT3& eye,const DirectX::XMFLOAT3& forward,float time,bool enabled);
    void begin(ID3D11DeviceContext* context);
    void bind_lighting(ID3D11DeviceContext* context);
    unsigned resolution()const;
    std::uint64_t depth_pixels(ID3D11Device* device,ID3D11DeviceContext* context);
};
