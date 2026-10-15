#pragma once
#include <d3d11.h>
#include <memory>
#include <string>

// Small authored status panel. Gameplay values come from the recovered Lua VM.
class MissionHud {
    struct Impl;std::unique_ptr<Impl> impl;
public:
    explicit MissionHud(ID3D11Device*);
    ~MissionHud();
    void draw(ID3D11DeviceContext*,const std::wstring&);
};
