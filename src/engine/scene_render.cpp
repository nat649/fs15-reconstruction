// Render recovered GIANTS meshes at their original hierarchical transforms.
#include "scene_render.h"
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <map>
#include <stdexcept>
#include <cstring>
#include <algorithm>
#include <chrono>
using Microsoft::WRL::ComPtr;
using namespace DirectX;
static void checked(HRESULT hr,const char* what){if(FAILED(hr))throw std::runtime_error(std::string(what)+": "+std::to_string(static_cast<unsigned long>(hr)));}
static constexpr const char* shaders=R"(
cbuffer View:register(b0){row_major float4x4 viewProjection;float4 eye;};
cbuffer Material:register(b1){float4 tint;};
struct In {float3 p:POSITION;float3 n:NORMAL;float2 uv:TEXCOORD0;float4 w0:WORLD0;float4 w1:WORLD1;float4 w2:WORLD2;float4 w3:WORLD3;float4 n0:NORM0;float4 n1:NORM1;float4 n2:NORM2;};
struct Out {float4 p:SV_POSITION;float3 world:TEXCOORD0;float3 n:TEXCOORD1;float2 uv:TEXCOORD2;};
Out vsMain(In v){Out o;float4x4 world=float4x4(v.w0,v.w1,v.w2,v.w3);float4 p=mul(float4(v.p,1),world);o.p=mul(p,viewProjection);o.world=p.xyz;o.n=mul(float4(v.n,0),float4x4(v.n0,v.n1,v.n2,float4(0,0,0,1))).xyz;o.uv=float2(v.uv.x,1-v.uv.y);return o;}
Texture2D diffuse:register(t0);SamplerState linearSampler:register(s0);
float4 psMain(Out v):SV_TARGET{float4 c=diffuse.Sample(linearSampler,v.uv)*tint;clip(c.a-0.35);float light=0.50+0.50*saturate(dot(normalize(v.n),normalize(float3(-0.3,0.8,-0.4))));float fog=saturate((distance(v.world,eye.xyz)-2000)/5000);return float4(lerp(c.rgb*light,float3(0.58,0.73,0.87),fog),1);}
)";
struct SceneRenderer::Impl {
    ID3D11DeviceContext* context;
    ComPtr<ID3D11VertexShader> vs;ComPtr<ID3D11PixelShader> ps;
    ComPtr<ID3D11InputLayout> layout;ComPtr<ID3D11Buffer> vertices,indices,instances,view,material;
    ComPtr<ID3D11SamplerState> sampler;
    std::map<std::filesystem::path,ComPtr<ID3D11ShaderResourceView>> textures;
    struct Draw {UINT first,count,base,instance_first,instance_count;fs15world::Material material;};
    std::vector<Draw> draws;
    ComPtr<ID3D11Query> query;
    struct Instance {float world[16],normal[12];};
    Impl(ID3D11Device* device,ID3D11DeviceContext* ctx,const fs15world::Terrain& scene,const std::filesystem::path& root):context(ctx){
        if(scene.groups.empty())return;
        D3D11_QUERY_DESC query_desc{};query_desc.Query=D3D11_QUERY_OCCLUSION;checked(device->CreateQuery(&query_desc,&query),"Original mesh visibility query");
        ComPtr<ID3DBlob> vertex,pixel,error;
        checked(D3DCompile(shaders,std::strlen(shaders),"recovered-mesh-renderer",nullptr,nullptr,"vsMain","vs_4_0",D3DCOMPILE_ENABLE_STRICTNESS,0,&vertex,&error),"Mesh vertex shader");
        checked(D3DCompile(shaders,std::strlen(shaders),"recovered-mesh-renderer",nullptr,nullptr,"psMain","ps_4_0",D3DCOMPILE_ENABLE_STRICTNESS,0,&pixel,&error),"Mesh pixel shader");
        checked(device->CreateVertexShader(vertex->GetBufferPointer(),vertex->GetBufferSize(),nullptr,&vs),"Mesh VS");
        checked(device->CreatePixelShader(pixel->GetBufferPointer(),pixel->GetBufferSize(),nullptr,&ps),"Mesh PS");
        const D3D11_INPUT_ELEMENT_DESC elements[]{
            {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},
            {"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0},
            {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,24,D3D11_INPUT_PER_VERTEX_DATA,0},
            {"WORLD",0,DXGI_FORMAT_R32G32B32A32_FLOAT,1,0,D3D11_INPUT_PER_INSTANCE_DATA,1},
            {"WORLD",1,DXGI_FORMAT_R32G32B32A32_FLOAT,1,16,D3D11_INPUT_PER_INSTANCE_DATA,1},
            {"WORLD",2,DXGI_FORMAT_R32G32B32A32_FLOAT,1,32,D3D11_INPUT_PER_INSTANCE_DATA,1},
            {"WORLD",3,DXGI_FORMAT_R32G32B32A32_FLOAT,1,48,D3D11_INPUT_PER_INSTANCE_DATA,1},
            {"NORM",0,DXGI_FORMAT_R32G32B32A32_FLOAT,1,64,D3D11_INPUT_PER_INSTANCE_DATA,1},
            {"NORM",1,DXGI_FORMAT_R32G32B32A32_FLOAT,1,80,D3D11_INPUT_PER_INSTANCE_DATA,1},
            {"NORM",2,DXGI_FORMAT_R32G32B32A32_FLOAT,1,96,D3D11_INPUT_PER_INSTANCE_DATA,1}};
        checked(device->CreateInputLayout(elements,10,vertex->GetBufferPointer(),vertex->GetBufferSize(),&layout),"Mesh input layout");
        auto buffer=[&](const void* data,size_t bytes,UINT bind,ComPtr<ID3D11Buffer>& target){
            if(bytes>0xffffffffu)throw std::runtime_error("Scene buffer exceeds D3D11 range");
            D3D11_BUFFER_DESC desc{};desc.ByteWidth=static_cast<UINT>(bytes);desc.BindFlags=bind;desc.Usage=data?D3D11_USAGE_IMMUTABLE:D3D11_USAGE_DEFAULT;
            D3D11_SUBRESOURCE_DATA initial{};initial.pSysMem=data;checked(device->CreateBuffer(&desc,data?&initial:nullptr,&target),"Mesh buffer");};
        std::vector<fs15world::Vertex> vv;std::vector<std::uint32_t> ii;std::vector<Instance> ww;
        std::vector<UINT> bases,firsts;
        for(const auto& mesh:scene.meshes){bases.push_back(static_cast<UINT>(vv.size()));firsts.push_back(static_cast<UINT>(ii.size()));vv.insert(vv.end(),mesh.vertices.begin(),mesh.vertices.end());ii.insert(ii.end(),mesh.indices.begin(),mesh.indices.end());}
        for(const auto& group:scene.groups){
            const auto& mesh=scene.meshes[group.mesh];UINT instance_first=static_cast<UINT>(ww.size());
            for(const auto& matrix:group.instances){Instance instance;std::copy(matrix.value,matrix.value+16,instance.world);XMFLOAT4X4 w;std::copy(matrix.value,matrix.value+16,&w._11);
                XMVECTOR determinant;auto normal=XMMatrixTranspose(XMMatrixInverse(&determinant,XMLoadFloat4x4(&w)));
                if(std::abs(XMVectorGetX(determinant))<1e-12f)throw std::runtime_error("Singular original scene transform");
                XMFLOAT4X4 n;XMStoreFloat4x4(&n,normal);std::copy(&n._11,&n._11+12,instance.normal);ww.push_back(instance);}
            for(size_t s=0;s<mesh.subsets.size();++s){
                if(s>=group.materials.size())throw std::runtime_error("Original subset/material count mismatch");
                auto found=scene.materials.find(group.materials[s]);if(found==scene.materials.end())throw std::runtime_error("Missing original material ID");
                auto mat=found->second;
                if(!textures.count(mat.texture)){
                    ComPtr<ID3D11Texture2D> texture;D3D11_TEXTURE2D_DESC desc{};desc.ArraySize=1;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
                    std::vector<D3D11_SUBRESOURCE_DATA> pixels;fs15world::Texture image;std::uint32_t white=0xffffffff;
                    if(mat.texture.empty()){desc.Width=desc.Height=desc.MipLevels=1;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;pixels.push_back({&white,4,0});}
                    else {
                        std::filesystem::path filename;for(auto prefix:{root/"assets",root/"recovered/gar"})if(std::filesystem::is_regular_file(prefix/mat.texture))filename=prefix/mat.texture;
                        if(filename.empty())throw std::runtime_error("Missing original diffuse: "+mat.texture.generic_u8string());
                        image=fs15world::load_dds(filename);desc.Width=image.width;desc.Height=image.height;desc.MipLevels=static_cast<UINT>(image.mip_offsets.size());
                        desc.Format=image.compression==1?DXGI_FORMAT_BC1_UNORM:image.compression==2?DXGI_FORMAT_BC2_UNORM:DXGI_FORMAT_BC3_UNORM;
                        for(size_t mip=0;mip<image.mip_offsets.size();++mip)pixels.push_back({image.bytes.data()+image.mip_offsets[mip],image.mip_pitches[mip],0});
                    }
                    checked(device->CreateTexture2D(&desc,pixels.data(),&texture),"Original diffuse texture");ComPtr<ID3D11ShaderResourceView> texture_view;
                    checked(device->CreateShaderResourceView(texture.Get(),nullptr,&texture_view),"Original diffuse view");textures.emplace(mat.texture,std::move(texture_view));
                }
                draws.push_back({firsts[group.mesh]+mesh.subsets[s].first,mesh.subsets[s].count,bases[group.mesh],instance_first,static_cast<UINT>(group.instances.size()),mat});
            }
        }
        buffer(vv.data(),vv.size()*sizeof(vv[0]),D3D11_BIND_VERTEX_BUFFER,vertices);buffer(ii.data(),ii.size()*4,D3D11_BIND_INDEX_BUFFER,indices);
        buffer(ww.data(),ww.size()*sizeof(Instance),D3D11_BIND_VERTEX_BUFFER,instances);buffer(nullptr,80,D3D11_BIND_CONSTANT_BUFFER,view);buffer(nullptr,16,D3D11_BIND_CONSTANT_BUFFER,material);
        D3D11_SAMPLER_DESC desc{};desc.Filter=D3D11_FILTER_ANISOTROPIC;desc.MaxAnisotropy=8;desc.AddressU=desc.AddressV=desc.AddressW=D3D11_TEXTURE_ADDRESS_WRAP;desc.MaxLOD=D3D11_FLOAT32_MAX;
        checked(device->CreateSamplerState(&desc,&sampler),"Original diffuse sampler");
    }
    void draw(const XMMATRIX& view_projection,const XMFLOAT3& eye,bool measure){
        if(draws.empty())return;
        struct View {XMFLOAT4X4 vp;XMFLOAT4 eye;} v;XMStoreFloat4x4(&v.vp,view_projection);v.eye={eye.x,eye.y,eye.z,1};context->UpdateSubresource(view.Get(),0,nullptr,&v,0,0);
        ID3D11Buffer* vb[]{vertices.Get(),instances.Get()};UINT strides[]{sizeof(fs15world::Vertex),sizeof(Instance)},offsets[]{0,0};
        context->IASetVertexBuffers(0,2,vb,strides,offsets);context->IASetIndexBuffer(indices.Get(),DXGI_FORMAT_R32_UINT,0);context->IASetInputLayout(layout.Get());
        context->VSSetShader(vs.Get(),nullptr,0);context->PSSetShader(ps.Get(),nullptr,0);
        auto b=view.Get();context->VSSetConstantBuffers(0,1,&b);context->PSSetConstantBuffers(0,1,&b);b=material.Get();context->PSSetConstantBuffers(1,1,&b);auto s=sampler.Get();context->PSSetSamplers(0,1,&s);
        if(measure)context->Begin(query.Get());
        for(const auto& d:draws){context->UpdateSubresource(material.Get(),0,nullptr,d.material.color,0,0);auto texture=textures.at(d.material.texture).Get();context->PSSetShaderResources(0,1,&texture);context->DrawIndexedInstanced(d.count,d.instance_count,d.first,d.base,d.instance_first);}
        if(measure)context->End(query.Get());
    }
    std::uint64_t visible_samples(){
        if(draws.empty())return 0;std::uint64_t samples=0;HRESULT result;context->Flush();auto timeout=std::chrono::steady_clock::now()+std::chrono::seconds(5);
        while((result=context->GetData(query.Get(),&samples,sizeof(samples),0))==S_FALSE){if(std::chrono::steady_clock::now()>timeout)throw std::runtime_error("Original mesh visibility timeout");Sleep(1);}
        checked(result,"Original mesh query");if(!samples)throw std::runtime_error("No original mesh pixels rasterized");return samples;
    }
};
SceneRenderer::SceneRenderer(ID3D11Device* d,ID3D11DeviceContext* c,const fs15world::Terrain& s,const std::filesystem::path& r):impl(std::make_unique<Impl>(d,c,s,r)){}
SceneRenderer::~SceneRenderer()=default;
void SceneRenderer::draw(const XMMATRIX& v,const XMFLOAT3& eye,bool measure){impl->draw(v,eye,measure);}
unsigned SceneRenderer::texture_count()const{return static_cast<unsigned>(impl->textures.size());}
std::uint64_t SceneRenderer::visible_samples(){return impl->visible_samples();}
