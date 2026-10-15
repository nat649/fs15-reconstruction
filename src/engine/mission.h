#pragma once
#include "resources.h"
#include <d3d11.h>
#include <DirectXMath.h>
#include <memory>

// Recovered gameplay scripts with an explicitly limited native service bridge.
class MissionRuntime {
    struct Impl;
    std::unique_ptr<Impl> impl;
public:
    MissionRuntime(const std::filesystem::path& root,const fs15world::Terrain& terrain,int difficulty);
    ~MissionRuntime();
    void initialize_graphics(ID3D11Device*,ID3D11DeviceContext*);
    void update(float dt,float throttle,float steering,bool handbrake);
    void action(const char* action);
    void draw(const DirectX::XMMATRIX&,const DirectX::XMFLOAT3&,bool shadow_pass=false,bool measure=false);
    bool camera(DirectX::XMFLOAT3& eye,DirectX::XMFLOAT3& target)const;
    void orbit(float yaw,float pitch);
    void zoom(float amount);
    std::wstring status()const;
    std::string report()const;
    std::uint64_t visible_samples();
    unsigned vehicle_count()const;
    const std::vector<std::filesystem::path>& resources()const;
};
