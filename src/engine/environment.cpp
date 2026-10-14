#include "environment.h"
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <xmllite.h>
#include <shlwapi.h>
#include <fstream>
#include <array>
#include <map>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <stdexcept>
using Microsoft::WRL::ComPtr;using namespace DirectX;
namespace fs=std::filesystem;
static void checked(HRESULT hr,const char* what){if(FAILED(hr))throw std::runtime_error(std::string(what)+": "+std::to_string(static_cast<unsigned long>(hr)));}
static std::string narrow(const wchar_t* p){int n=WideCharToMultiByte(CP_UTF8,0,p,-1,nullptr,0,nullptr,nullptr);std::string s(n,0);WideCharToMultiByte(CP_UTF8,0,p,-1,s.data(),n,nullptr,nullptr);s.pop_back();return s;}
static std::string attribute(IXmlReader* xml,const wchar_t* key){auto hr=xml->MoveToAttributeByName(key,nullptr);if(hr==S_FALSE)return {};checked(hr,"Sky attribute");const wchar_t* v;checked(xml->GetValue(&v,nullptr),"Sky value");auto result=narrow(v);xml->MoveToElement();return result;}
struct EnvironmentRenderer::Impl {
    std::vector<std::array<float,15>> curves;
    std::vector<fs::path> used;
    ComPtr<ID3D11VertexShader> vs;ComPtr<ID3D11PixelShader> ps;ComPtr<ID3D11InputLayout> layout;
    ComPtr<ID3D11Buffer> vb,ib,constants;ComPtr<ID3D11SamplerState> sampler;ComPtr<ID3D11DepthStencilState> depth;
    struct Draw {unsigned first,count;std::array<ComPtr<ID3D11ShaderResourceView>,5> textures;};std::vector<Draw> draws;
    Impl(ID3D11Device* device,const fs::path& root){
        fs::path file;for(auto prefix:{root/"assets",root/"recovered/gar"})if(fs::is_regular_file(prefix/"data/sky/environment.curves"))file=prefix/"data/sky/environment.curves";
        if(file.empty())return; // Authored standalone terrain/mesh fixtures have no sky.
        std::ifstream in(file,std::ios::binary);char magic[8];std::uint32_t count=0;in.read(magic,8);in.read(reinterpret_cast<char*>(&count),4);
        if(std::string(magic,8)!="FS15ENV1"||count!=1441)throw std::runtime_error("Invalid recovered environment curve cache");
        curves.resize(count);in.read(reinterpret_cast<char*>(curves.data()),curves.size()*sizeof(curves[0]));if(!in||in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Truncated environment curve cache");
        for(auto& row:curves)for(float x:row)if(!std::isfinite(x))throw std::runtime_error("Invalid environment curve value");
        used={"data/sky/environment.curves","data/sky/sky_day_night.i3d","data/sky/sky_day_night.meshes","data/sky/skyShader.xml"};
        auto sky_meshes=fs15world::load_meshes(root,"data/sky/sky_day_night.meshes");if(sky_meshes.size()!=1)throw std::runtime_error("Expected one original sky mesh");
        auto& mesh=sky_meshes[0];std::map<int,std::string> files;std::map<int,std::array<fs::path,5>> materials;int material=0;std::vector<int> ids;
        auto scene=fs15world::resolve_resource(root,"data/sky/sky_day_night.i3d");ComPtr<IStream> input;ComPtr<IXmlReader> xml;
        checked(SHCreateStreamOnFileEx(scene.c_str(),STGM_READ|STGM_SHARE_DENY_WRITE,0,FALSE,nullptr,&input),"Sky I3D");checked(CreateXmlReader(__uuidof(IXmlReader),reinterpret_cast<void**>(xml.GetAddressOf()),nullptr),"Sky reader");
        checked(xml->SetProperty(XmlReaderProperty_DtdProcessing,DtdProcessing_Prohibit),"Sky DTD rejection");checked(xml->SetInput(input.Get()),"Sky stream");
        XmlNodeType type;HRESULT hr;
        while((hr=xml->Read(&type))==S_OK){
            const wchar_t* name=nullptr;if(type!=XmlNodeType_Element&&type!=XmlNodeType_EndElement)continue;checked(xml->GetLocalName(&name,nullptr),"Sky tag");std::wstring tag=name;
            if(type==XmlNodeType_EndElement){if(tag==L"Material")material=0;continue;}
            if(tag==L"File")files.emplace(std::stoi(attribute(xml.Get(),L"fileId")),attribute(xml.Get(),L"filename"));
            else if(tag==L"Material")material=std::stoi(attribute(xml.Get(),L"materialId"));
            else if(material&&(tag==L"Emissivemap"||tag==L"Custommap")){
                auto name_value=attribute(xml.Get(),L"name");unsigned slot=tag==L"Emissivemap"?0:name_value=="sunsetTexture"?1:name_value=="nightTexture"?2:name_value=="sunriseTexture"?3:4;
                auto path=fs::path("data/sky")/fs::u8path(files.at(std::stoi(attribute(xml.Get(),L"fileId"))));path.replace_extension(".dds");materials[material][slot]=path;
            }else if(tag==L"Shape"){auto text=attribute(xml.Get(),L"materialIds");std::replace(text.begin(),text.end(),',',' ');std::istringstream list(text);int id;while(list>>id)ids.push_back(id);}
        }checked(hr,"Sky parse");if(ids.size()!=mesh.subsets.size())throw std::runtime_error("Original sky material/subset mismatch");
        for(size_t part=0;part<ids.size();++part){Draw draw{mesh.subsets[part].first,mesh.subsets[part].count,{}};
            for(unsigned slot=0;slot<5;++slot){auto path=materials.at(ids[part])[slot];auto image=fs15world::load_dds(fs15world::resolve_resource(root,path));used.push_back(path);
                D3D11_TEXTURE2D_DESC d{};d.Width=image.width;d.Height=image.height;d.ArraySize=1;d.MipLevels=static_cast<UINT>(image.mip_offsets.size());d.SampleDesc.Count=1;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;d.Usage=D3D11_USAGE_IMMUTABLE;
                d.Format=image.compression==1?DXGI_FORMAT_BC1_UNORM_SRGB:image.compression==2?DXGI_FORMAT_BC2_UNORM_SRGB:DXGI_FORMAT_BC3_UNORM_SRGB;
                std::vector<D3D11_SUBRESOURCE_DATA> p;for(size_t m=0;m<image.mip_offsets.size();++m)p.push_back({image.bytes.data()+image.mip_offsets[m],image.mip_pitches[m],0});
                ComPtr<ID3D11Texture2D> texture;checked(device->CreateTexture2D(&d,p.data(),&texture),"Original sky DDS");checked(device->CreateShaderResourceView(texture.Get(),nullptr,&draw.textures[slot]),"Sky SRV");
            }draws.push_back(std::move(draw));
        }
        auto source=std::string(R"(
cbuffer Sky:register(b0){row_major float4x4 vp;float4 partScale;float4 rainScale;};
Texture2D dayTexture:register(t0);Texture2D sunsetTexture:register(t1);Texture2D nightTexture:register(t2);Texture2D sunriseTexture:register(t3);Texture2D rainTexture:register(t4);SamplerState linearSampler:register(s0);
struct Input{float3 p:POSITION;float2 uv:TEXCOORD0;};struct Output{float4 position:SV_POSITION;float2 uv:TEXCOORD0;};
Output vsMain(Input v){Output Out;Out.position=mul(float4(v.p,1),vp);Out.uv=float2(v.uv.x,1-v.uv.y);
)");
        // Both original injection bodies are compiled unchanged; only host
        // variable/sampler bindings are supplied around them.
        source+=fs15world::shader_injection(root,"data/sky/skyShader.xml","VS_END");
        source+=R"(return Out;}
struct LegacyVS{float2 ALBEDOMAP_TEXCOORD;};struct LegacyInput{LegacyVS vs;};
#define tex2D(texture,coords) texture.Sample(linearSampler,coords)
float4 psMain(Output v):SV_TARGET{LegacyInput In;In.vs.ALBEDOMAP_TEXCOORD=v.uv;float4 emissiveColor=dayTexture.Sample(linearSampler,v.uv);
)";
        source+=fs15world::shader_injection(root,"data/sky/skyShader.xml","EMISSIVE_FS");source+="return float4(emissiveColor.xyz,1);}";
        auto compile=[&](const char* entry,const char* profile){ComPtr<ID3DBlob> code,error;HRESULT result=D3DCompile(source.data(),source.size(),"original-GE6-sky",nullptr,nullptr,entry,profile,D3DCOMPILE_ENABLE_STRICTNESS,0,&code,&error);if(FAILED(result))throw std::runtime_error(error?std::string(static_cast<char*>(error->GetBufferPointer()),error->GetBufferSize()):"Sky shader compile failed");return code;};
        auto vertex=compile("vsMain","vs_4_0"),pixel=compile("psMain","ps_4_0");checked(device->CreateVertexShader(vertex->GetBufferPointer(),vertex->GetBufferSize(),nullptr,&vs),"Sky VS");checked(device->CreatePixelShader(pixel->GetBufferPointer(),pixel->GetBufferSize(),nullptr,&ps),"Sky PS");
        D3D11_INPUT_ELEMENT_DESC elements[]{{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},{"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,24,D3D11_INPUT_PER_VERTEX_DATA,0}};
        checked(device->CreateInputLayout(elements,2,vertex->GetBufferPointer(),vertex->GetBufferSize(),&layout),"Sky layout");
        auto buffer=[&](const void* data,size_t size,UINT bind,ComPtr<ID3D11Buffer>& b){D3D11_BUFFER_DESC d{};d.ByteWidth=static_cast<UINT>(size);d.BindFlags=bind;d.Usage=data?D3D11_USAGE_IMMUTABLE:D3D11_USAGE_DEFAULT;D3D11_SUBRESOURCE_DATA p{};p.pSysMem=data;checked(device->CreateBuffer(&d,data?&p:nullptr,&b),"Sky buffer");};
        buffer(mesh.vertices.data(),mesh.vertices.size()*sizeof(fs15world::Vertex),D3D11_BIND_VERTEX_BUFFER,vb);buffer(mesh.indices.data(),mesh.indices.size()*4,D3D11_BIND_INDEX_BUFFER,ib);buffer(nullptr,96,D3D11_BIND_CONSTANT_BUFFER,constants);
        D3D11_SAMPLER_DESC s{};s.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;s.AddressU=s.AddressV=s.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;s.MaxLOD=D3D11_FLOAT32_MAX;checked(device->CreateSamplerState(&s,&sampler),"Sky sampler");
        D3D11_DEPTH_STENCIL_DESC d{};d.DepthEnable=TRUE;d.DepthFunc=D3D11_COMPARISON_LESS_EQUAL;d.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ZERO;checked(device->CreateDepthStencilState(&d,&depth),"Sky depth state");
    }
    EnvironmentSample sample(float hour)const{
        EnvironmentSample s;if(curves.empty())return s;float minute=std::fmod(hour*60,1440.0f);if(minute<0)minute+=1440;unsigned first=static_cast<unsigned>(minute);float a=minute-first;std::array<float,15> v;
        for(unsigned i=0;i<15;++i)v[i]=curves[first][i]*(1-a)+curves[first+1][i]*a;
        s.sky={v[0],v[1],v[2],v[3]};s.ambient={v[4],v[5],v[6],0};s.diffuse={v[7],v[8],v[9],0};s.fog={v[10],v[11],v[12],v[13]};
        auto rotation=XMMatrixRotationX(v[14])*XMMatrixRotationY(XMConvertToRadians(45))*XMMatrixRotationZ(XMConvertToRadians(25));XMStoreFloat3(&s.light,XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(0,0,1,0),rotation)));return s;
    }
};
EnvironmentRenderer::EnvironmentRenderer(ID3D11Device* d,const fs::path& r):impl(std::make_unique<Impl>(d,r)){}
EnvironmentRenderer::~EnvironmentRenderer()=default;
bool EnvironmentRenderer::ready()const{return !impl->curves.empty();}
EnvironmentSample EnvironmentRenderer::sample(float h)const{return impl->sample(h);}
const std::vector<fs::path>& EnvironmentRenderer::resources()const{return impl->used;}
void EnvironmentRenderer::draw(ID3D11DeviceContext* c,const XMMATRIX& vp,const XMFLOAT3& eye,float hour){
    if(!ready())return;struct Constants{XMFLOAT4X4 vp;XMFLOAT4 sky,rain;} data;XMStoreFloat4x4(&data.vp,XMMatrixTranslation(eye.x,eye.y,eye.z)*vp);data.sky=impl->sample(hour).sky;data.rain={0,0,0,0};c->UpdateSubresource(impl->constants.Get(),0,nullptr,&data,0,0);
    UINT stride=sizeof(fs15world::Vertex),offset=0;auto vb=impl->vb.Get();c->IASetVertexBuffers(0,1,&vb,&stride,&offset);c->IASetIndexBuffer(impl->ib.Get(),DXGI_FORMAT_R32_UINT,0);c->IASetInputLayout(impl->layout.Get());c->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    c->VSSetShader(impl->vs.Get(),nullptr,0);c->PSSetShader(impl->ps.Get(),nullptr,0);auto b=impl->constants.Get();c->VSSetConstantBuffers(0,1,&b);c->PSSetConstantBuffers(0,1,&b);auto s=impl->sampler.Get();c->PSSetSamplers(0,1,&s);c->OMSetDepthStencilState(impl->depth.Get(),0);
    for(const auto& draw:impl->draws){ID3D11ShaderResourceView* views[5];for(unsigned i=0;i<5;++i)views[i]=draw.textures[i].Get();c->PSSetShaderResources(0,5,views);c->DrawIndexed(draw.count,draw.first,0);}c->OMSetDepthStencilState(nullptr,0);
}
