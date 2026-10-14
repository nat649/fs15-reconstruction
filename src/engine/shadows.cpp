// D3D11 shadow service using original I3D sunlight settings. This is one local
// orthographic map, not recovery of the complete GIANTS cascade implementation.
#include "shadows.h"
#include <wrl/client.h>
#include <stdexcept>
#include <algorithm>
#include <cmath>
using Microsoft::WRL::ComPtr;using namespace DirectX;
static void checked(HRESULT hr){if(FAILED(hr))throw std::runtime_error("D3D11 shadow service failed: "+std::to_string(static_cast<unsigned long>(hr)));}
struct ShadowRenderer::Impl {
    fs15world::Sun sun;bool enabled=false;
    ComPtr<ID3D11Texture2D> texture;ComPtr<ID3D11DepthStencilView> depth;ComPtr<ID3D11ShaderResourceView> view;
    ComPtr<ID3D11SamplerState> sampler;ComPtr<ID3D11RasterizerState> raster;ComPtr<ID3D11Buffer> constants;
    Impl(ID3D11Device* device,const fs15world::Sun& settings):sun(settings){
        D3D11_TEXTURE2D_DESC d{};d.Width=d.Height=sun.resolution;d.MipLevels=d.ArraySize=1;d.SampleDesc.Count=1;d.Format=DXGI_FORMAT_R32_TYPELESS;d.BindFlags=D3D11_BIND_DEPTH_STENCIL|D3D11_BIND_SHADER_RESOURCE;
        checked(device->CreateTexture2D(&d,nullptr,&texture));D3D11_DEPTH_STENCIL_VIEW_DESC dd{};dd.Format=DXGI_FORMAT_D32_FLOAT;dd.ViewDimension=D3D11_DSV_DIMENSION_TEXTURE2D;checked(device->CreateDepthStencilView(texture.Get(),&dd,&depth));
        D3D11_SHADER_RESOURCE_VIEW_DESC v{};v.Format=DXGI_FORMAT_R32_FLOAT;v.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;v.Texture2D.MipLevels=1;checked(device->CreateShaderResourceView(texture.Get(),&v,&view));
        D3D11_SAMPLER_DESC s{};s.Filter=D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;s.AddressU=s.AddressV=s.AddressW=D3D11_TEXTURE_ADDRESS_BORDER;s.BorderColor[0]=s.BorderColor[1]=s.BorderColor[2]=s.BorderColor[3]=1;s.ComparisonFunc=D3D11_COMPARISON_LESS_EQUAL;s.MaxLOD=D3D11_FLOAT32_MAX;checked(device->CreateSamplerState(&s,&sampler));
        D3D11_RASTERIZER_DESC r{};r.CullMode=D3D11_CULL_NONE;r.FillMode=D3D11_FILL_SOLID;r.DepthClipEnable=TRUE;r.DepthBias=16;r.SlopeScaledDepthBias=sun.slope;checked(device->CreateRasterizerState(&r,&raster));
        D3D11_BUFFER_DESC b{};b.ByteWidth=144;b.BindFlags=D3D11_BIND_CONSTANT_BUFFER;b.Usage=D3D11_USAGE_DEFAULT;checked(device->CreateBuffer(&b,nullptr,&constants));
    }
};
ShadowRenderer::ShadowRenderer(ID3D11Device* d,const fs15world::Sun& s):impl(std::make_unique<Impl>(d,s)){}
ShadowRenderer::~ShadowRenderer()=default;
XMMATRIX ShadowRenderer::update(ID3D11DeviceContext* c,const EnvironmentSample& e,const XMFLOAT3& eye,const XMFLOAT3& forward,float time,bool enabled){
    impl->enabled=enabled&&impl->sun.shadows;
    const float distance=impl->sun.distance,extrusion=impl->sun.extrusion;
    auto center=XMVectorSet(eye.x+forward.x*distance*0.5f,eye.y+forward.y*distance*0.5f,eye.z+forward.z*distance*0.5f,1);
    auto light=XMVector3Normalize(XMLoadFloat3(&e.light));auto position=center+light*(extrusion+distance);
    auto up=std::abs(e.light.y)>0.98f?XMVectorSet(1,0,0,0):XMVectorSet(0,1,0,0);
    auto vp=XMMatrixLookAtRH(position,center,up)*XMMatrixOrthographicRH(distance*2,distance*2,1,(extrusion+distance)*2);
    struct Data{XMFLOAT4X4 matrix;XMFLOAT4 light,diffuse,ambient,fog,frame;} data;
    XMStoreFloat4x4(&data.matrix,vp);data.light={e.light.x,e.light.y,e.light.z,0};data.diffuse=e.diffuse;data.ambient=e.ambient;data.fog=e.fog;
    // Original bias is adapted to this single-map D3D depth range; its exact
    // GIANTS cascade/bias mapping has not been recovered.
    data.frame={time,impl->enabled?1.0f:0.0f,1.0f/impl->sun.resolution,std::clamp(impl->sun.bias*0.02f,0.00001f,0.0001f)};
    c->UpdateSubresource(impl->constants.Get(),0,nullptr,&data,0,0);auto b=impl->constants.Get();c->VSSetConstantBuffers(2,1,&b);c->PSSetConstantBuffers(2,1,&b);return vp;
}
void ShadowRenderer::begin(ID3D11DeviceContext* c){ID3D11ShaderResourceView* empty=nullptr;c->PSSetShaderResources(10,1,&empty);c->OMSetRenderTargets(0,nullptr,impl->depth.Get());c->ClearDepthStencilView(impl->depth.Get(),D3D11_CLEAR_DEPTH,1,0);c->OMSetDepthStencilState(nullptr,0);D3D11_VIEWPORT viewport{0,0,float(impl->sun.resolution),float(impl->sun.resolution),0,1};c->RSSetViewports(1,&viewport);c->RSSetState(impl->raster.Get());}
void ShadowRenderer::bind_lighting(ID3D11DeviceContext* c){auto v=impl->view.Get();auto s=impl->sampler.Get();c->PSSetShaderResources(10,1,&v);c->PSSetSamplers(2,1,&s);}
unsigned ShadowRenderer::resolution()const{return impl->sun.resolution;}
std::uint64_t ShadowRenderer::depth_pixels(ID3D11Device* d,ID3D11DeviceContext* c){
    if(!impl->enabled)return 0;D3D11_TEXTURE2D_DESC desc{};impl->texture->GetDesc(&desc);desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> staging;checked(d->CreateTexture2D(&desc,nullptr,&staging));c->CopyResource(staging.Get(),impl->texture.Get());D3D11_MAPPED_SUBRESOURCE mapped{};checked(c->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped));
    std::uint64_t count=0;for(unsigned y=0;y<desc.Height;++y){auto row=reinterpret_cast<const float*>(static_cast<const unsigned char*>(mapped.pData)+y*mapped.RowPitch);for(unsigned x=0;x<desc.Width;++x)if(row[x]<0.99999f)++count;}c->Unmap(staging.Get(),0);return count;
}
