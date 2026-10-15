#include "hud.h"
#include <windows.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <stdexcept>
using Microsoft::WRL::ComPtr;
static void check_hud(HRESULT hr){if(FAILED(hr))throw std::runtime_error("Mission HUD graphics initialization failed");}
struct MissionHud::Impl {
    ComPtr<ID3D11Texture2D> texture;ComPtr<ID3D11ShaderResourceView> view;
    ComPtr<ID3D11VertexShader> vs;ComPtr<ID3D11PixelShader> ps;
    ComPtr<ID3D11SamplerState> sampler;ComPtr<ID3D11DepthStencilState> depth;
    HDC dc=nullptr;HBITMAP bitmap=nullptr;HFONT font=nullptr;HGDIOBJ old_bitmap=nullptr,old_font=nullptr;
    void* pixels=nullptr;std::wstring previous;
    static constexpr unsigned w=1000,h=82;
    explicit Impl(ID3D11Device* device){
        const char* source=R"(
struct Out{float4 p:SV_POSITION;float2 uv:TEXCOORD0;};
Out vsMain(uint id:SV_VertexID){float2 p=float2((id<<1)&2,id&2);Out o;o.p=float4(-1+p.x*2000.0/1280.0,1-p.y*164.0/720.0,0,1);o.uv=p;return o;}
Texture2D panel:register(t0);SamplerState pointSampler:register(s0);
float4 psMain(Out p):SV_TARGET{clip(1-p.uv.x);clip(1-p.uv.y);return float4(panel.Sample(pointSampler,p.uv).rgb,1);}
)";
        ComPtr<ID3DBlob> vertex,pixel,error;
        check_hud(D3DCompile(source,strlen(source),"mission-status",nullptr,nullptr,"vsMain","vs_4_0",0,0,&vertex,&error));
        check_hud(D3DCompile(source,strlen(source),"mission-status",nullptr,nullptr,"psMain","ps_4_0",0,0,&pixel,&error));
        check_hud(device->CreateVertexShader(vertex->GetBufferPointer(),vertex->GetBufferSize(),nullptr,&vs));
        check_hud(device->CreatePixelShader(pixel->GetBufferPointer(),pixel->GetBufferSize(),nullptr,&ps));
        D3D11_TEXTURE2D_DESC t{};t.Width=w;t.Height=h;t.MipLevels=t.ArraySize=t.SampleDesc.Count=1;t.Format=DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;t.Usage=D3D11_USAGE_DEFAULT;t.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        check_hud(device->CreateTexture2D(&t,nullptr,&texture));check_hud(device->CreateShaderResourceView(texture.Get(),nullptr,&view));
        D3D11_SAMPLER_DESC s{};s.Filter=D3D11_FILTER_MIN_MAG_MIP_POINT;s.AddressU=s.AddressV=s.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;check_hud(device->CreateSamplerState(&s,&sampler));
        D3D11_DEPTH_STENCIL_DESC d{};d.DepthEnable=FALSE;check_hud(device->CreateDepthStencilState(&d,&depth));
        dc=CreateCompatibleDC(nullptr);BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=w;info.bmiHeader.biHeight=-int(h);info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
        bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);font=CreateFontW(-18,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        if(!dc||!bitmap||!font)throw std::runtime_error("Mission HUD text surface unavailable");old_bitmap=SelectObject(dc,bitmap);old_font=SelectObject(dc,font);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(245,245,245));
    }
    ~Impl(){if(dc){if(old_font)SelectObject(dc,old_font);if(old_bitmap)SelectObject(dc,old_bitmap);}if(font)DeleteObject(font);if(bitmap)DeleteObject(bitmap);if(dc)DeleteDC(dc);}
    void draw(ID3D11DeviceContext* c,const std::wstring& text){
        if(previous!=text){RECT area{0,0,w,h};auto brush=CreateSolidBrush(RGB(20,25,28));FillRect(dc,&area,brush);DeleteObject(brush);area={10,5,w-10,h-4};DrawTextW(dc,text.c_str(),int(text.size()),&area,DT_LEFT|DT_TOP|DT_NOPREFIX);GdiFlush();c->UpdateSubresource(texture.Get(),0,nullptr,pixels,w*4,0);previous=text;}
        ComPtr<ID3D11DepthStencilState> old;UINT stencil=0;c->OMGetDepthStencilState(&old,&stencil);c->OMSetDepthStencilState(depth.Get(),0);
        c->IASetInputLayout(nullptr);c->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);c->VSSetShader(vs.Get(),nullptr,0);c->PSSetShader(ps.Get(),nullptr,0);auto resource=view.Get();c->PSSetShaderResources(0,1,&resource);auto s=sampler.Get();c->PSSetSamplers(0,1,&s);c->Draw(3,0);c->OMSetDepthStencilState(old.Get(),stencil);
    }
};
MissionHud::MissionHud(ID3D11Device* d):impl(std::make_unique<Impl>(d)){}
MissionHud::~MissionHud()=default;
void MissionHud::draw(ID3D11DeviceContext* c,const std::wstring& s){impl->draw(c,s);}
