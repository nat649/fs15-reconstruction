// Original I3D terrain weights, diffuse resources and metre-based repeat sizes.
// Simplified blend; GIANTS priority/parallax/normal/crop shaders remain separate.
#include "terrain_material.h"
#include <wrl/client.h>
#include <array>
#include <stdexcept>
using Microsoft::WRL::ComPtr;
static void checked(HRESULT hr){if(FAILED(hr))throw std::runtime_error("Original terrain material upload failed: "+std::to_string(static_cast<unsigned long>(hr)));}
struct TerrainMaterial::Impl {
    std::array<ComPtr<ID3D11ShaderResourceView>,9> views;
    ComPtr<ID3D11Buffer> settings;ComPtr<ID3D11SamplerState> repeat;
    unsigned count=0;
    Impl(ID3D11Device* device,const fs15world::Terrain& terrain,const std::filesystem::path& root){
        struct Settings{float sizes[8]{1,1,1,1,1,1,1,1};float options[4]{};} data;
        // No layers is supported for original terrain-only fixtures.
        std::vector<fs15world::WeightImage> weights;
        auto resolve=[&](const std::filesystem::path& p){for(auto prefix:{root/"assets",root/"recovered/gar"})if(std::filesystem::is_regular_file(prefix/p))return prefix/p;throw std::runtime_error("Missing original terrain layer: "+p.generic_u8string());};
        for(const auto& layer:terrain.layers){
            weights.push_back(fs15world::load_weight(resolve(layer.weight)));auto image=fs15world::load_dds(resolve(layer.detail));data.sizes[count]=layer.unit_size;
            D3D11_TEXTURE2D_DESC desc{};desc.Width=image.width;desc.Height=image.height;desc.MipLevels=static_cast<UINT>(image.mip_offsets.size());desc.ArraySize=1;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
            desc.Format=image.compression==1?DXGI_FORMAT_BC1_UNORM_SRGB:image.compression==2?DXGI_FORMAT_BC2_UNORM_SRGB:DXGI_FORMAT_BC3_UNORM_SRGB;
            std::vector<D3D11_SUBRESOURCE_DATA> pixels;for(size_t m=0;m<image.mip_offsets.size();++m)pixels.push_back({image.bytes.data()+image.mip_offsets[m],image.mip_pitches[m],0});
            ComPtr<ID3D11Texture2D> texture;checked(device->CreateTexture2D(&desc,pixels.data(),&texture));checked(device->CreateShaderResourceView(texture.Get(),nullptr,&views[1+count]));++count;
        }
        if(count){
            D3D11_TEXTURE2D_DESC desc{};desc.Width=weights[0].width;desc.Height=weights[0].height;desc.MipLevels=1;desc.ArraySize=count;desc.SampleDesc.Count=1;desc.Format=DXGI_FORMAT_R8_UNORM;desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
            std::vector<D3D11_SUBRESOURCE_DATA> pixels;for(const auto& w:weights){if(w.width!=desc.Width||w.height!=desc.Height)throw std::runtime_error("Terrain weights have different sizes");pixels.push_back({w.pixels.data(),w.width,0});}
            ComPtr<ID3D11Texture2D> texture;checked(device->CreateTexture2D(&desc,pixels.data(),&texture));checked(device->CreateShaderResourceView(texture.Get(),nullptr,&views[0]));
        }
        data.options[0]=float(count);D3D11_BUFFER_DESC b{};b.ByteWidth=sizeof(data);b.BindFlags=D3D11_BIND_CONSTANT_BUFFER;b.Usage=D3D11_USAGE_IMMUTABLE;D3D11_SUBRESOURCE_DATA initial{};initial.pSysMem=&data;checked(device->CreateBuffer(&b,&initial,&settings));
        D3D11_SAMPLER_DESC s{};s.Filter=D3D11_FILTER_ANISOTROPIC;s.MaxAnisotropy=8;s.AddressU=s.AddressV=s.AddressW=D3D11_TEXTURE_ADDRESS_WRAP;s.MaxLOD=D3D11_FLOAT32_MAX;checked(device->CreateSamplerState(&s,&repeat));
    }
};
TerrainMaterial::TerrainMaterial(ID3D11Device* d,const fs15world::Terrain& t,const std::filesystem::path& r):impl(std::make_unique<Impl>(d,t,r)){}
TerrainMaterial::~TerrainMaterial()=default;
unsigned TerrainMaterial::layer_count()const{return impl->count;}
void TerrainMaterial::bind(ID3D11DeviceContext* c){ID3D11ShaderResourceView* v[9];for(unsigned i=0;i<9;++i)v[i]=impl->views[i].Get();c->PSSetShaderResources(1,9,v);auto b=impl->settings.Get();c->PSSetConstantBuffers(1,1,&b);auto s=impl->repeat.Get();c->PSSetSamplers(1,1,&s);}
