// Render recovered GIANTS meshes at their original hierarchical transforms.
#include "scene_render.h"
#include "render_shared.h"
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <map>
#include <stdexcept>
#include <cstring>
#include <algorithm>
#include <chrono>
#include <set>
using Microsoft::WRL::ComPtr;
using namespace DirectX;
static void checked(HRESULT hr,const char* what){if(FAILED(hr))throw std::runtime_error(std::string(what)+": "+std::to_string(static_cast<unsigned long>(hr)));}
static constexpr const char* shaders=R"(
cbuffer View:register(b0){row_major float4x4 viewProjection;float4 eye;};
cbuffer Material:register(b1){float4 tint;float4 animationParams;float4 effectInfo;};
struct In {float3 p:POSITION;float3 n:NORMAL;float2 uv:TEXCOORD0;float4 w0:WORLD0;float4 w1:WORLD1;float4 w2:WORLD2;float4 w3:WORLD3;float4 n0:NORM0;float4 n1:NORM1;float4 n2:NORM2;};
struct Out {float4 p:SV_POSITION;float3 world:TEXCOORD0;float3 n:TEXCOORD1;float2 uv:TEXCOORD2;};
Out vsMain(In v){Out o;float4x4 world=float4x4(v.w0,v.w1,v.w2,v.w3);GE6Input legacy;legacy.position=float4(v.p,1);legacy.uv=v.uv;float4 local=legacy.position;
if(effectInfo.x==1)local=ge6Wind(legacy,animationParams,frameInfo.x);if(effectInfo.x==2)local=ge6Flag(legacy,animationParams,frameInfo.x);
float4 p=mul(local,world);o.p=mul(p,viewProjection);o.world=p.xyz;o.n=mul(float4(v.n,0),float4x4(v.n0,v.n1,v.n2,float4(0,0,0,1))).xyz;o.uv=float2(v.uv.x,1-v.uv.y);return o;}
Texture2D diffuse:register(t0);SamplerState linearSampler:register(s0);
float4 psMain(Out v):SV_TARGET{float4 c=diffuse.Sample(linearSampler,v.uv)*tint;clip(c.a-0.35);float3 normal=normalize(v.n);
float3 lit=effectInfo.x==3?ge6Branch(normal,sunDirection.xyz,1,sunDiffuse.xyz):saturate(dot(normal,sunDirection.xyz))*sunDiffuse.xyz;
float visibility=effectInfo.y>0.5?shadowVisibility(v.world):1;
return float4(applyDistanceFog(c.rgb*(ambientColor.xyz+lit*visibility),v.world,eye.xyz),1);}
void psShadow(Out v){float alpha=diffuse.Sample(linearSampler,v.uv).a*tint.a;clip(alpha-0.35);}
)";
struct SceneRenderer::Impl {
    ComPtr<ID3D11Device> graphics_device;
    ID3D11DeviceContext* context;
    ComPtr<ID3D11VertexShader> vs;ComPtr<ID3D11PixelShader> ps,shadow_ps;
    ComPtr<ID3D11InputLayout> layout;ComPtr<ID3D11Buffer> vertices,indices,instances,view,material;
    ComPtr<ID3D11SamplerState> sampler;
    std::map<std::filesystem::path,ComPtr<ID3D11ShaderResourceView>> textures;
    struct Draw {UINT first,count,base,instance_first,instance_count;fs15world::Material material;bool casts,receives;};
    std::vector<Draw> draws;
    std::vector<unsigned> draw_groups;
    unsigned injection_count=0,animated=0;
    ComPtr<ID3D11Query> query;
    struct Instance {float world[16],normal[12];};
    Impl(ID3D11Device* graphics,ID3D11DeviceContext* ctx,const fs15world::Terrain& scene,const std::filesystem::path& root):graphics_device(graphics),context(ctx){
        auto device=graphics;
        if(scene.groups.empty())return;
        D3D11_QUERY_DESC query_desc{};query_desc.Query=D3D11_QUERY_OCCLUSION;checked(device->CreateQuery(&query_desc,&query),"Original mesh visibility query");
        std::set<unsigned> effects;for(const auto& group:scene.groups)for(int id:group.materials)effects.insert(scene.materials.at(id).effect);
        std::string source=world_shader_services;
        source+="struct GE6Input{float4 position;float2 uv;};float2 getDefaultTexCoords(GE6Input In){return In.uv;}\n";
        for(unsigned effect=1;effect<=3;++effect){
            if(effect==1)source+="float4 ge6Wind(GE6Input In,float4 directionAndFrequency,float cTime_s){\n";
            else if(effect==2)source+="float4 ge6Flag(GE6Input In,float4 displacementSpeedFrequencyAndTexOff,float cTime_s){\n";
            else source+="float3 ge6Branch(float3 normal,float3 lightDir,float atten,float3 lightColor){\n";
            if(effects.count(effect)){
                const auto path=effect==1?"data/maps/shaders/windShader.xml":effect==2?"data/maps/shaders/flagShader.xml":"data/maps/shaders/treeBranchShader.xml";
                source+=fs15world::shader_injection(root,path,effect==3?"GET_DIFFUSE_LIGHTING_FS":"GET_POSITION_VS");++injection_count;
            }else source+=effect==3?"return saturate(dot(normal,lightDir))*lightColor;":"return In.position;";
            source+="}\n";
        }
        source+=shaders;
        auto compile=[&](const char* entry,const char* profile){ComPtr<ID3DBlob> code,error;auto hr=D3DCompile(source.data(),source.size(),"original-GE6-mesh-injections",nullptr,nullptr,entry,profile,D3DCOMPILE_ENABLE_STRICTNESS,0,&code,&error);if(FAILED(hr))throw std::runtime_error(error?std::string(static_cast<char*>(error->GetBufferPointer()),error->GetBufferSize()):"Mesh shader compile failed");return code;};
        auto vertex=compile("vsMain","vs_4_0"),pixel=compile("psMain","ps_4_0"),shadow=compile("psShadow","ps_4_0");
        checked(device->CreateVertexShader(vertex->GetBufferPointer(),vertex->GetBufferSize(),nullptr,&vs),"Mesh VS");
        checked(device->CreatePixelShader(pixel->GetBufferPointer(),pixel->GetBufferSize(),nullptr,&ps),"Mesh PS");
        checked(device->CreatePixelShader(shadow->GetBufferPointer(),shadow->GetBufferSize(),nullptr,&shadow_ps),"Mesh shadow PS");
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
            D3D11_BUFFER_DESC desc{};desc.ByteWidth=static_cast<UINT>(bytes);desc.BindFlags=bind;desc.Usage=D3D11_USAGE_DEFAULT;
            D3D11_SUBRESOURCE_DATA initial{};initial.pSysMem=data;checked(device->CreateBuffer(&desc,data?&initial:nullptr,&target),"Mesh buffer");};
        std::vector<fs15world::Vertex> vv;std::vector<std::uint32_t> ii;std::vector<Instance> ww;
        std::vector<UINT> bases,firsts;
        for(const auto& mesh:scene.meshes){bases.push_back(static_cast<UINT>(vv.size()));firsts.push_back(static_cast<UINT>(ii.size()));vv.insert(vv.end(),mesh.vertices.begin(),mesh.vertices.end());ii.insert(ii.end(),mesh.indices.begin(),mesh.indices.end());}
        unsigned group_index=0;for(const auto& group:scene.groups){
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
                        desc.Format=image.compression==1?DXGI_FORMAT_BC1_UNORM_SRGB:image.compression==2?DXGI_FORMAT_BC2_UNORM_SRGB:DXGI_FORMAT_BC3_UNORM_SRGB;
                        for(size_t mip=0;mip<image.mip_offsets.size();++mip)pixels.push_back({image.bytes.data()+image.mip_offsets[mip],image.mip_pitches[mip],0});
                    }
                    checked(device->CreateTexture2D(&desc,pixels.data(),&texture),"Original diffuse texture");ComPtr<ID3D11ShaderResourceView> texture_view;
                    checked(device->CreateShaderResourceView(texture.Get(),nullptr,&texture_view),"Original diffuse view");textures.emplace(mat.texture,std::move(texture_view));
                }
                draws.push_back({firsts[group.mesh]+mesh.subsets[s].first,mesh.subsets[s].count,bases[group.mesh],instance_first,static_cast<UINT>(group.instances.size()),mat,group.casts_shadow,group.receives_shadow});
                draw_groups.push_back(group_index);
                if(mat.effect==1||mat.effect==2)++animated;
            }
            ++group_index;
        }
        buffer(vv.data(),vv.size()*sizeof(vv[0]),D3D11_BIND_VERTEX_BUFFER,vertices);buffer(ii.data(),ii.size()*4,D3D11_BIND_INDEX_BUFFER,indices);
        buffer(ww.data(),ww.size()*sizeof(Instance),D3D11_BIND_VERTEX_BUFFER,instances);buffer(nullptr,80,D3D11_BIND_CONSTANT_BUFFER,view);buffer(nullptr,48,D3D11_BIND_CONSTANT_BUFFER,material);
        D3D11_SAMPLER_DESC desc{};desc.Filter=D3D11_FILTER_ANISOTROPIC;desc.MaxAnisotropy=8;desc.AddressU=desc.AddressV=desc.AddressW=D3D11_TEXTURE_ADDRESS_WRAP;desc.MaxLOD=D3D11_FLOAT32_MAX;
        checked(device->CreateSamplerState(&desc,&sampler),"Original diffuse sampler");
    }
    void draw(const XMMATRIX& view_projection,const XMFLOAT3& eye,bool measure,bool shadow_pass){
        if(draws.empty())return;
        struct View {XMFLOAT4X4 vp;XMFLOAT4 eye;} v;XMStoreFloat4x4(&v.vp,view_projection);v.eye={eye.x,eye.y,eye.z,1};context->UpdateSubresource(view.Get(),0,nullptr,&v,0,0);
        ID3D11Buffer* vb[]{vertices.Get(),instances.Get()};UINT strides[]{sizeof(fs15world::Vertex),sizeof(Instance)},offsets[]{0,0};
        context->IASetVertexBuffers(0,2,vb,strides,offsets);context->IASetIndexBuffer(indices.Get(),DXGI_FORMAT_R32_UINT,0);context->IASetInputLayout(layout.Get());
        context->VSSetShader(vs.Get(),nullptr,0);context->PSSetShader(shadow_pass?shadow_ps.Get():ps.Get(),nullptr,0);
        auto b=view.Get();context->VSSetConstantBuffers(0,1,&b);context->PSSetConstantBuffers(0,1,&b);b=material.Get();context->VSSetConstantBuffers(1,1,&b);context->PSSetConstantBuffers(1,1,&b);auto s=sampler.Get();context->PSSetSamplers(0,1,&s);
        if(measure)context->Begin(query.Get());
        for(const auto& d:draws){if(shadow_pass&&!d.casts)continue;struct Material{float tint[4],params[4],effect[4];} m{};
            std::copy(d.material.color,d.material.color+4,m.tint);std::copy(d.material.effect_params,d.material.effect_params+4,m.params);m.effect[0]=float(d.material.effect);m.effect[1]=d.receives?1.0f:0;
            context->UpdateSubresource(material.Get(),0,nullptr,&m,0,0);auto texture=textures.at(d.material.texture).Get();context->PSSetShaderResources(0,1,&texture);context->DrawIndexedInstanced(d.count,d.instance_count,d.first,d.base,d.instance_first);}
        if(measure)context->End(query.Get());
    }
    void update_instances(const fs15world::Terrain& scene){
        if(!instances)return;std::vector<Instance> values;
        for(const auto& group:scene.groups)for(const auto& matrix:group.instances){Instance v;std::copy(matrix.value,matrix.value+16,v.world);XMFLOAT4X4 w;std::copy(matrix.value,matrix.value+16,&w._11);XMVECTOR det;auto m=XMMatrixTranspose(XMMatrixInverse(&det,XMLoadFloat4x4(&w)));if(std::abs(XMVectorGetX(det))<1e-12f)throw std::runtime_error("Singular dynamic scene transform");XMFLOAT4X4 n;XMStoreFloat4x4(&n,m);std::copy(&n._11,&n._11+12,v.normal);values.push_back(v);}
        std::vector<UINT> firsts;UINT offset=0;for(const auto& g:scene.groups){firsts.push_back(offset);offset+=UINT(g.instances.size());}
        for(size_t i=0;i<draws.size();++i){auto group=draw_groups[i];draws[i].instance_first=firsts.at(group);draws[i].instance_count=UINT(scene.groups.at(group).instances.size());}
        if(values.empty())return;D3D11_BUFFER_DESC desc{};instances->GetDesc(&desc);const auto size=values.size()*sizeof(Instance);
        if(size>UINT_MAX)throw std::runtime_error("Dynamic scene instance buffer exceeds limit");
        if(size>desc.ByteWidth){instances.Reset();desc.ByteWidth=UINT(size);D3D11_SUBRESOURCE_DATA initial{};initial.pSysMem=values.data();checked(graphics_device->CreateBuffer(&desc,&initial,&instances),"Growing dynamic instances");}
        else {D3D11_BOX box{0,0,0,UINT(size),1,1};context->UpdateSubresource(instances.Get(),0,&box,values.data(),0,0);}
    }
    std::uint64_t visible_samples(bool require_visible){
        if(draws.empty())return 0;std::uint64_t samples=0;HRESULT result;context->Flush();auto timeout=std::chrono::steady_clock::now()+std::chrono::seconds(5);
        while((result=context->GetData(query.Get(),&samples,sizeof(samples),0))==S_FALSE){if(std::chrono::steady_clock::now()>timeout)throw std::runtime_error("Original mesh visibility timeout");Sleep(1);}
        checked(result,"Original mesh query");if(!samples&&require_visible)throw std::runtime_error("No original mesh pixels rasterized");return samples;
    }
};
SceneRenderer::SceneRenderer(ID3D11Device* d,ID3D11DeviceContext* c,const fs15world::Terrain& s,const std::filesystem::path& r):impl(std::make_unique<Impl>(d,c,s,r)){}
SceneRenderer::~SceneRenderer()=default;
void SceneRenderer::update_instances(const fs15world::Terrain& s){impl->update_instances(s);}
void SceneRenderer::draw(const XMMATRIX& v,const XMFLOAT3& eye,bool measure,bool shadow_pass){impl->draw(v,eye,measure,shadow_pass);}
unsigned SceneRenderer::texture_count()const{return static_cast<unsigned>(impl->textures.size());}
unsigned SceneRenderer::original_injections()const{return impl->injection_count;}
unsigned SceneRenderer::animated_draws()const{return impl->animated;}
std::uint64_t SceneRenderer::visible_samples(bool require_visible){return impl->visible_samples(require_visible);}
