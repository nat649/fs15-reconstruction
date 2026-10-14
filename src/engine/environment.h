#pragma once
#include "resources.h"
#include <d3d11.h>
#include <DirectXMath.h>
#include <memory>
struct EnvironmentSample {
    DirectX::XMFLOAT4 sky{1,0,0,0},ambient{0.5f,0.5f,0.5f,0},diffuse{0.5f,0.5f,0.5f,0},fog{0.58f,0.73f,0.87f,0};
    DirectX::XMFLOAT3 light{-0.3f,0.8f,-0.4f};
};
class EnvironmentRenderer {
    struct Impl;std::unique_ptr<Impl> impl;
public:
    EnvironmentRenderer(ID3D11Device* device,const std::filesystem::path& root);
    ~EnvironmentRenderer();
    EnvironmentSample sample(float hour)const;
    void draw(ID3D11DeviceContext* context,const DirectX::XMMATRIX& vp,const DirectX::XMFLOAT3& eye,float hour);
    bool ready()const;
    const std::vector<std::filesystem::path>& resources()const;
};
