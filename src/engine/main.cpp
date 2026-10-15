// Independently compiled engine bootstrap: actual I3D terrain data, D3D11 and
// free camera. Original binary meshes are staged by the local resource bridge.
#include <windows.h>
#include <windowsx.h>
#include <objbase.h>
#include <shellapi.h>
#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <iomanip>
#include "resources.h"
#include "scene_render.h"
#include "terrain_material.h"
#include "environment.h"
#include "shadows.h"
#include "render_shared.h"
#include "mission.h"
#include "hud.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
namespace fs=std::filesystem;
static constexpr unsigned width=1280,height=720;
static bool smoke=false,done=false,rmb=false;
static float environment_hour=8,animation_time=0;
static bool shadows_enabled=true;
static std::array<bool,256> keys{};
static POINT last_mouse{};
static std::ofstream log_file;
static std::unique_ptr<MissionRuntime> mission;
static std::vector<const char*> mission_actions;
static bool follow_vehicle=true;
static void log(const std::string& message){log_file<<message<<'\n';log_file.flush();OutputDebugStringA((message+"\n").c_str());}
static void check(HRESULT result,const char* operation){if(FAILED(result))throw std::runtime_error(std::string(operation)+" failed (HRESULT "+std::to_string(static_cast<unsigned long>(result))+")");}
static std::wstring wide(const std::string& text){if(text.empty())return {};int n=MultiByteToWideChar(CP_UTF8,0,text.data(),int(text.size()),nullptr,0);std::wstring out(n,0);MultiByteToWideChar(CP_UTF8,0,text.data(),int(text.size()),out.data(),n);return out;}

struct Camera {
    XMFLOAT3 position{};
    float yaw=0,pitch=0;
    XMFLOAT3 home{};float home_yaw=0,home_pitch=0;
    void initialize(const fs15world::Terrain& terrain,bool original=false) {
        position={terrain.width*0.85f,terrain.max_height+terrain.width*0.6f,-terrain.depth*0.85f};
        const float dx=-position.x,dy=(terrain.min_height+terrain.max_height)*0.5f-position.y,dz=-position.z;
        yaw=std::atan2(dx,dz);pitch=std::atan2(dy,std::sqrt(dx*dx+dz*dz));home=position;home_yaw=yaw;home_pitch=pitch;
        if(original&&!terrain.meshes.empty())for(const auto& node:terrain.nodes)if(node.type=="Camera"){
            position={node.world[12],node.world[13],node.world[14]};
            yaw=std::atan2(-node.world[8],-node.world[10]);pitch=std::atan2(-node.world[9],std::sqrt(node.world[8]*node.world[8]+node.world[10]*node.world[10]));
            home=position;home_yaw=yaw;home_pitch=pitch;break;
        }
    }
    void reset(){position=home;yaw=home_yaw;pitch=home_pitch;}
    void look_at(const XMFLOAT3& eye,const XMFLOAT3& target){position=eye;float dx=target.x-eye.x,dy=target.y-eye.y,dz=target.z-eye.z;if(dx*dx+dy*dy+dz*dz<0.001f)throw std::runtime_error("Camera target equals position");yaw=std::atan2(dx,dz);pitch=std::atan2(dy,std::sqrt(dx*dx+dz*dz));home=position;home_yaw=yaw;home_pitch=pitch;}
    void move(float forward,float side,float rise,float dt) {
        position.x+=(std::sin(yaw)*std::cos(pitch)*forward-std::cos(yaw)*side)*dt;
        position.y+=(std::sin(pitch)*forward+rise)*dt;
        position.z+=(std::cos(yaw)*std::cos(pitch)*forward+std::sin(yaw)*side)*dt;
    }
    XMMATRIX view()const {
        const auto eye=XMLoadFloat3(&position);
        const auto direction=XMVectorSet(std::sin(yaw)*std::cos(pitch),std::sin(pitch),std::cos(yaw)*std::cos(pitch),0);
        return XMMatrixLookToRH(eye,direction,XMVectorSet(0,1,0,0));
    }
    void update(float dt) {
        float speed=keys[VK_SHIFT]?480.0f:150.0f;
        float forward=float(keys['W']||keys['Z']||keys[VK_UP])-float(keys['S']||keys[VK_DOWN]);
        float side=float(keys['D']||keys[VK_RIGHT])-float(keys['A']||keys['Q']||keys[VK_LEFT]);
        float rise=float(keys[VK_SPACE]||keys[VK_PRIOR])-float(keys[VK_CONTROL]||keys[VK_NEXT]);
        move(forward*speed,side*speed,rise*speed,dt);
    }
} camera;

static constexpr const char* shader=R"hlsl(
cbuffer View : register(b0) { row_major float4x4 viewProjection; float4 eye; };
struct Input { float3 position:POSITION; float3 normal:NORMAL; float2 uv:TEXCOORD0; };
struct Output { float4 position:SV_POSITION; float3 world:TEXCOORD0; float3 normal:TEXCOORD1; float2 uv:TEXCOORD2; };
Output vsMain(Input v) { Output o; o.position=mul(float4(v.position,1),viewProjection); o.world=v.position;o.normal=v.normal;o.uv=v.uv;return o; }
Texture2D overview:register(t0);SamplerState linearSampler:register(s0);
cbuffer Layers:register(b1){float4 unitSize0;float4 unitSize1;float4 layerOptions;};
Texture2DArray weights:register(t1);
Texture2D detail0:register(t2);Texture2D detail1:register(t3);Texture2D detail2:register(t4);Texture2D detail3:register(t5);
Texture2D detail4:register(t6);Texture2D detail5:register(t7);Texture2D detail6:register(t8);Texture2D detail7:register(t9);
SamplerState repeatSampler:register(s1);
#define BLEND_LAYER(i,tex,size) if(layerOptions.x>i){float w=weights.Sample(linearSampler,float3(v.uv,i)).r;sum+=w*tex.Sample(repeatSampler,v.world.xz/size).rgb;total+=w;}
float4 psMain(Output v):SV_TARGET {
 float3 albedo=overview.Sample(linearSampler,v.uv).rgb;
 if(layerOptions.x>0){float3 sum=0;float total=0;
 BLEND_LAYER(0,detail0,unitSize0.x) BLEND_LAYER(1,detail1,unitSize0.y) BLEND_LAYER(2,detail2,unitSize0.z) BLEND_LAYER(3,detail3,unitSize0.w)
 BLEND_LAYER(4,detail4,unitSize1.x) BLEND_LAYER(5,detail5,unitSize1.y) BLEND_LAYER(6,detail6,unitSize1.z) BLEND_LAYER(7,detail7,unitSize1.w)
 albedo=total>0.001?sum/total:detail0.Sample(repeatSampler,v.world.xz/unitSize0.x).rgb;}
 float3 light=ambientColor.xyz+saturate(dot(normalize(v.normal),sunDirection.xyz))*sunDiffuse.xyz*shadowVisibility(v.world);
 return float4(applyDistanceFog(albedo*light,v.world,eye.xyz),1);
})hlsl";

class Renderer {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGISwapChain> swap;
    ComPtr<ID3D11Texture2D> back,depth;
    ComPtr<ID3D11RenderTargetView> color_view;
    ComPtr<ID3D11DepthStencilView> depth_view;
    ComPtr<ID3D11VertexShader> vertex_shader;
    ComPtr<ID3D11PixelShader> pixel_shader;
    ComPtr<ID3D11InputLayout> layout;
    ComPtr<ID3D11Buffer> vertices,indices,constants;
    ComPtr<ID3D11ShaderResourceView> texture_view;
    ComPtr<ID3D11SamplerState> sampler;
    ComPtr<ID3D11RasterizerState> raster;
    ComPtr<ID3D11Query> query;
    unsigned index_count=0;
    std::unique_ptr<SceneRenderer> scene;
    std::unique_ptr<TerrainMaterial> terrain_material;
    std::unique_ptr<EnvironmentRenderer> environment;
    std::unique_ptr<ShadowRenderer> shadows;
    bool has_shadows=false;
    std::unique_ptr<MissionHud> hud;
public:
    D3D_FEATURE_LEVEL feature_level{};
    bool software=false;
    std::uint64_t samples=0;
    std::uint64_t object_samples=0;
    std::uint64_t shadow_pixels=0;
    bool environment_ready()const{return environment->ready();}
    unsigned original_injections()const{return scene->original_injections()+(environment->ready()?2:0);}
    unsigned animated_draws()const{return scene->animated_draws();}
    unsigned shadow_resolution()const{return shadows->resolution();}
    const std::vector<fs::path>& environment_resources()const{return environment->resources();}
    Renderer(HWND window,const fs15world::Terrain& terrain,bool force_warp,const fs::path& root) {
        DXGI_SWAP_CHAIN_DESC desc{};desc.BufferDesc.Width=width;desc.BufferDesc.Height=height;desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count=1;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.BufferCount=1;desc.OutputWindow=window;desc.Windowed=TRUE;desc.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
        const D3D_FEATURE_LEVEL levels[]{D3D_FEATURE_LEVEL_11_0,D3D_FEATURE_LEVEL_10_1,D3D_FEATURE_LEVEL_10_0};
        auto create=[&](D3D_DRIVER_TYPE driver){return D3D11CreateDeviceAndSwapChain(nullptr,driver,nullptr,0,levels,3,D3D11_SDK_VERSION,&desc,&swap,&device,&feature_level,&context);};
        HRESULT created=force_warp?E_FAIL:create(D3D_DRIVER_TYPE_HARDWARE);
        if(FAILED(created)){swap.Reset();device.Reset();context.Reset();software=true;created=create(D3D_DRIVER_TYPE_WARP);}
        check(created,"D3D11 device/swap chain");
        log(std::string("DEVICE_READY driver=")+(software?"WARP":"hardware")+" feature="+std::to_string(feature_level));
        check(swap->GetBuffer(0,IID_PPV_ARGS(&back)),"Back buffer");D3D11_RENDER_TARGET_VIEW_DESC srgb{};srgb.Format=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;srgb.ViewDimension=D3D11_RTV_DIMENSION_TEXTURE2D;check(device->CreateRenderTargetView(back.Get(),&srgb,&color_view),"sRGB color view");
        D3D11_TEXTURE2D_DESC depth_desc{};depth_desc.Width=width;depth_desc.Height=height;depth_desc.MipLevels=1;depth_desc.ArraySize=1;
        depth_desc.Format=DXGI_FORMAT_D24_UNORM_S8_UINT;depth_desc.SampleDesc.Count=1;depth_desc.BindFlags=D3D11_BIND_DEPTH_STENCIL;
        check(device->CreateTexture2D(&depth_desc,nullptr,&depth),"Depth buffer");check(device->CreateDepthStencilView(depth.Get(),nullptr,&depth_view),"Depth view");
        ComPtr<ID3DBlob> vs,ps,errors;
        auto compile=[&](const char* entry,const char* profile,ComPtr<ID3DBlob>& bytecode){
            const auto source=std::string(world_shader_services)+shader;
            HRESULT result=D3DCompile(source.data(),source.size(),"fs15world/terrain-shader",nullptr,nullptr,entry,profile,
                D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&bytecode,&errors);
            if(FAILED(result))throw std::runtime_error(errors?std::string(static_cast<const char*>(errors->GetBufferPointer()),errors->GetBufferSize()):"Shader compilation failed");
        };
        compile("vsMain","vs_4_0",vs);compile("psMain","ps_4_0",ps);
        check(device->CreateVertexShader(vs->GetBufferPointer(),vs->GetBufferSize(),nullptr,&vertex_shader),"Vertex shader");
        check(device->CreatePixelShader(ps->GetBufferPointer(),ps->GetBufferSize(),nullptr,&pixel_shader),"Pixel shader");
        D3D11_INPUT_ELEMENT_DESC elements[]{
            {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},
            {"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0},
            {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,24,D3D11_INPUT_PER_VERTEX_DATA,0}};
        check(device->CreateInputLayout(elements,3,vs->GetBufferPointer(),vs->GetBufferSize(),&layout),"Terrain input layout");
        auto buffer=[&](const void* data,size_t size,UINT bind,ComPtr<ID3D11Buffer>& out){
            if(!size||size>UINT_MAX)throw std::runtime_error("Invalid GPU buffer size");
            D3D11_BUFFER_DESC b{};b.ByteWidth=static_cast<UINT>(size);b.Usage=D3D11_USAGE_DEFAULT;b.BindFlags=bind;
            D3D11_SUBRESOURCE_DATA initial{};initial.pSysMem=data;check(device->CreateBuffer(&b,data?&initial:nullptr,&out),"Terrain GPU buffer");
        };
        buffer(terrain.vertices.data(),terrain.vertices.size()*sizeof(fs15world::Vertex),D3D11_BIND_VERTEX_BUFFER,vertices);
        buffer(terrain.indices.data(),terrain.indices.size()*sizeof(std::uint32_t),D3D11_BIND_INDEX_BUFFER,indices);
        index_count=static_cast<unsigned>(terrain.indices.size());buffer(nullptr,80,D3D11_BIND_CONSTANT_BUFFER,constants);
        auto image=fs15world::load_dds(terrain.texture_file);
        D3D11_TEXTURE2D_DESC texture_desc{};texture_desc.Width=image.width;texture_desc.Height=image.height;texture_desc.MipLevels=static_cast<unsigned>(image.mip_offsets.size());texture_desc.ArraySize=1;
        texture_desc.Format=image.compression==1?DXGI_FORMAT_BC1_UNORM_SRGB:image.compression==2?DXGI_FORMAT_BC2_UNORM_SRGB:DXGI_FORMAT_BC3_UNORM_SRGB;
        texture_desc.SampleDesc.Count=1;texture_desc.Usage=D3D11_USAGE_IMMUTABLE;texture_desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        std::vector<D3D11_SUBRESOURCE_DATA> pixels(image.mip_offsets.size());
        for(size_t i=0;i<pixels.size();++i){pixels[i].pSysMem=image.bytes.data()+image.mip_offsets[i];pixels[i].SysMemPitch=image.mip_pitches[i];}
        ComPtr<ID3D11Texture2D> texture;check(device->CreateTexture2D(&texture_desc,pixels.data(),&texture),"Original overview texture");
        check(device->CreateShaderResourceView(texture.Get(),nullptr,&texture_view),"Terrain texture view");
        D3D11_SAMPLER_DESC s{};s.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;s.AddressU=s.AddressV=s.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;s.MaxLOD=D3D11_FLOAT32_MAX;
        check(device->CreateSamplerState(&s,&sampler),"Terrain sampler");
        D3D11_RASTERIZER_DESC r{};r.FillMode=D3D11_FILL_SOLID;r.CullMode=D3D11_CULL_NONE;r.DepthClipEnable=TRUE;
        check(device->CreateRasterizerState(&r,&raster),"Rasterizer");
        D3D11_QUERY_DESC q{};q.Query=D3D11_QUERY_OCCLUSION;check(device->CreateQuery(&q,&query),"GPU visibility query");
        log("GPU_RESOURCES_READY vertices="+std::to_string(terrain.vertices.size())+" triangles="+std::to_string(index_count/3));
        scene=std::make_unique<SceneRenderer>(device.Get(),context.Get(),terrain,root);
        terrain_material=std::make_unique<TerrainMaterial>(device.Get(),terrain,root);
        environment=std::make_unique<EnvironmentRenderer>(device.Get(),root);shadows=std::make_unique<ShadowRenderer>(device.Get(),terrain.sun);has_shadows=terrain.sun.shadows;
        log("ENVIRONMENT_READY recovered_sky="+std::to_string(environment->ready())+" original_injections="+std::to_string(original_injections())+" animated_draws="+std::to_string(animated_draws()));
        log("TERRAIN_LAYERS_READY count="+std::to_string(terrain_material->layer_count()));
        log("SCENE_GPU_READY meshes="+std::to_string(terrain.meshes.size())+" instances="+std::to_string(terrain.rendered_instances)+" textures="+std::to_string(scene->texture_count()));
        if(mission){mission->initialize_graphics(device.Get(),context.Get());hud=std::make_unique<MissionHud>(device.Get());}
    }
    void draw(bool measure=false,bool present=true) {
        struct View { XMFLOAT4X4 matrix;XMFLOAT4 eye; } data;
        const auto projection=XMMatrixPerspectiveFovRH(XM_PI/3,float(width)/height,1.0f,10000.0f);
        XMFLOAT3 forward{std::sin(camera.yaw)*std::cos(camera.pitch),std::sin(camera.pitch),std::cos(camera.yaw)*std::cos(camera.pitch)};
        const auto lighting=environment->sample(environment_hour);
        const auto shadow_vp=shadows->update(context.Get(),lighting,camera.position,forward,animation_time,shadows_enabled);
        auto bind_terrain=[&](){UINT stride=sizeof(fs15world::Vertex),offset=0;auto vb=vertices.Get();context->IASetVertexBuffers(0,1,&vb,&stride,&offset);context->IASetIndexBuffer(indices.Get(),DXGI_FORMAT_R32_UINT,0);context->IASetInputLayout(layout.Get());context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);context->VSSetShader(vertex_shader.Get(),nullptr,0);auto b=constants.Get();context->VSSetConstantBuffers(0,1,&b);};
        if(shadows_enabled&&has_shadows){
            shadows->begin(context.Get());XMStoreFloat4x4(&data.matrix,shadow_vp);data.eye={camera.position.x,camera.position.y,camera.position.z,1};context->UpdateSubresource(constants.Get(),0,nullptr,&data,0,0);bind_terrain();context->PSSetShader(nullptr,nullptr,0);context->DrawIndexed(index_count,0,0);scene->draw(shadow_vp,camera.position,false,true);if(mission)mission->draw(shadow_vp,camera.position,true);
        }
        XMStoreFloat4x4(&data.matrix,camera.view()*projection);data.eye={camera.position.x,camera.position.y,camera.position.z,1};
        context->UpdateSubresource(constants.Get(),0,nullptr,&data,0,0);
        const float sky[]{0.58f,0.73f,0.87f,1};context->ClearRenderTargetView(color_view.Get(),sky);context->ClearDepthStencilView(depth_view.Get(),D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL,1,0);
        auto target=color_view.Get();context->OMSetRenderTargets(1,&target,depth_view.Get());
        D3D11_VIEWPORT viewport{0,0,float(width),float(height),0,1};context->RSSetViewports(1,&viewport);context->RSSetState(raster.Get());
        environment->draw(context.Get(),camera.view()*projection,camera.position,environment_hour);
        UINT stride=sizeof(fs15world::Vertex),offset=0;auto vertex_buffer=vertices.Get();context->IASetVertexBuffers(0,1,&vertex_buffer,&stride,&offset);
        context->IASetIndexBuffer(indices.Get(),DXGI_FORMAT_R32_UINT,0);context->IASetInputLayout(layout.Get());context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->VSSetShader(vertex_shader.Get(),nullptr,0);context->PSSetShader(pixel_shader.Get(),nullptr,0);
        auto c=constants.Get();context->VSSetConstantBuffers(0,1,&c);context->PSSetConstantBuffers(0,1,&c);
        auto t=texture_view.Get();auto s=sampler.Get();context->PSSetShaderResources(0,1,&t);context->PSSetSamplers(0,1,&s);
        terrain_material->bind(context.Get());
        shadows->bind_lighting(context.Get());
        if(measure)context->Begin(query.Get());context->DrawIndexed(index_count,0,0);if(measure)context->End(query.Get());scene->draw(camera.view()*projection,camera.position,measure);if(mission){mission->draw(camera.view()*projection,camera.position,false,measure);hud->draw(context.Get(),mission->status());}
        if(present)check(swap->Present(smoke?0:1,0),"Present 3D frame");
    }
    void verify_visibility() {
        context->Flush();auto end=std::chrono::steady_clock::now()+std::chrono::seconds(5);HRESULT result;
        while((result=context->GetData(query.Get(),&samples,sizeof(samples),0))==S_FALSE) {
            if(std::chrono::steady_clock::now()>end)throw std::runtime_error("GPU visibility query timed out");Sleep(1);
        }
        check(result,"GPU visibility samples");if(!samples)throw std::runtime_error("No terrain geometry rasterized");
        log("TERRAIN_VISIBLE gpu_samples="+std::to_string(samples));
        object_samples=scene->visible_samples();log("ORIGINAL_MESHES_VISIBLE gpu_samples="+std::to_string(object_samples));
        shadow_pixels=shadows->depth_pixels(device.Get(),context.Get());if(shadows_enabled&&has_shadows&&!shadow_pixels)throw std::runtime_error("Shadow depth map is empty");
        log("SHADOW_DEPTH_READY pixels="+std::to_string(shadow_pixels));
    }
    void capture(const fs::path& filename) {
        D3D11_TEXTURE2D_DESC d{};back->GetDesc(&d);d.BindFlags=0;d.Usage=D3D11_USAGE_STAGING;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;d.MiscFlags=0;
        ComPtr<ID3D11Texture2D> staging;check(device->CreateTexture2D(&d,nullptr,&staging),"Readback texture");context->CopyResource(staging.Get(),back.Get());
        D3D11_MAPPED_SUBRESOURCE mapped{};check(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped),"Readback pixels");
        std::vector<std::uint8_t> bgra(size_t(width)*height*4);auto source=static_cast<const std::uint8_t*>(mapped.pData);
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){auto p=source+y*mapped.RowPitch+x*4;auto o=bgra.data()+(size_t(y)*width+x)*4;o[0]=p[2];o[1]=p[1];o[2]=p[0];o[3]=255;}
        context->Unmap(staging.Get(),0);fs15world::save_png(filename,width,height,bgra.data(),width*4);
    }
};

static LRESULT CALLBACK window_proc(HWND window,UINT message,WPARAM wparam,LPARAM lparam) {
    switch(message) {
    case WM_KEYDOWN:
        if(wparam<keys.size())keys[wparam]=true;
        if(wparam==VK_ESCAPE)DestroyWindow(window);if(wparam==VK_HOME)camera.reset();
        if(!(lparam&(1L<<30))){
            if(wparam==VK_F5)environment_hour=std::fmod(environment_hour+1,24.0f);
            if(wparam==VK_F6)environment_hour=std::fmod(environment_hour+23,24.0f);
            if(wparam==VK_F7)shadows_enabled=!shadows_enabled;
            if(mission){const std::pair<WPARAM,const char*> actions[]{{VK_TAB,"next"},{'E',"attach"},{'R',"fill"},{'I',"tip"},{'B',"work"},{'O',"pipe"},{'P',"pause"},{VK_F8,"camera"}};for(auto action:actions)if(wparam==action.first)mission_actions.push_back(action.second);}
        }return 0;
    case WM_KEYUP:if(wparam<keys.size())keys[wparam]=false;return 0;
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:rmb=true;last_mouse={GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam)};SetCapture(window);return 0;
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:rmb=false;ReleaseCapture();return 0;
    case WM_MOUSEMOVE:if(rmb){POINT now{GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam)};if(mission&&follow_vehicle)mission->orbit(-(now.x-last_mouse.x)*0.004f,(now.y-last_mouse.y)*0.004f);else{camera.yaw-=(now.x-last_mouse.x)*0.004f;camera.pitch=std::clamp(camera.pitch-(now.y-last_mouse.y)*0.004f,-1.5f,1.5f);}last_mouse=now;}return 0;
    case WM_MOUSEWHEEL:if(mission&&follow_vehicle)mission->zoom(-GET_WHEEL_DELTA_WPARAM(wparam)/120.0f);return 0;
    case WM_KILLFOCUS:keys.fill(false);mission_actions.clear();rmb=false;ReleaseCapture();if(mission)mission->set_paused(true);return 0;
    case WM_DESTROY:done=true;PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(window,message,wparam,lparam);
}
static fs::path exe_directory(){wchar_t path[32768];auto n=GetModuleFileNameW(nullptr,path,32768);if(!n||n>=32768)throw std::runtime_error("Engine executable path unavailable");return fs::path(std::wstring(path,n)).parent_path();}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show) {
    SetProcessDPIAware();HRESULT com=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);HWND window=nullptr;int result=0;
    try {
        check(com,"Engine COM initialization");auto root=exe_directory();if(!fs::exists(root/"assets/data/maps/map01.i3d")&&!fs::exists(root/"recovered/gar/data/maps/map01.i3d"))root=root.parent_path().parent_path();
        std::string map="Map01",scenario;fs::path capture,report;bool warp=false,original_view=true,play=false,mission_test=false,scene_only=false;int difficulty=2;int argc=0;auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);
        bool reference_bytecode=false,test_pause=false;unsigned test_frame_rate=60;
        XMFLOAT3 explicit_camera{},explicit_target{};bool has_camera=false,has_target=false;
        for(int i=1;i<argc;++i){std::wstring arg=argv[i];
            if(arg==L"--root"&&i+1<argc)root=fs::absolute(argv[++i]);
            else if(arg==L"--map"&&i+1<argc){std::wstring m=argv[++i];if(m!=L"Map01"&&m!=L"Map02")throw std::runtime_error("Only Map01/Map02 are supported");map=m==L"Map01"?"Map01":"Map02";}
            else if(arg==L"--smoke")smoke=true;
            else if(arg==L"--play")play=true;
            else if(arg==L"--scene")scene_only=true;
            else if(arg==L"--mission-test"){play=true;mission_test=true;}
            else if(arg==L"--reference-bytecode")reference_bytecode=true;
            else if(arg==L"--test-pause")test_pause=true;
            else if(arg==L"--test-frame-rate"&&i+1<argc){size_t end;std::wstring text=argv[++i];int rate=std::stoi(text,&end);if(end!=text.size()||rate<5||rate>240)throw std::runtime_error("Test frame rate must be 5..240");test_frame_rate=unsigned(rate);}
            else if(arg==L"--scenario"&&i+1<argc){std::wstring s=argv[++i];if(s!=L"harvest"&&s!=L"silo"&&s!=L"tip")throw std::runtime_error("Scenario must be harvest, silo or tip");scenario=s==L"harvest"?"harvest":s==L"tip"?"tip":"silo";play=true;}
            else if(arg==L"--difficulty"&&i+1<argc){difficulty=std::stoi(argv[++i]);if(difficulty<1||difficulty>3)throw std::runtime_error("Difficulty must be 1..3");}
            else if(arg==L"--warp")warp=true;
            else if(arg==L"--no-shadows")shadows_enabled=false;
            else if((arg==L"--hour"||arg==L"--time")&&i+1<argc){size_t end;std::wstring text=argv[++i];float value=std::stof(text,&end);if(end!=text.size()||!std::isfinite(value)||value<0||(arg==L"--hour"&&value>=24))throw std::runtime_error("Invalid environment time");if(arg==L"--hour")environment_hour=value;else animation_time=value;}
            else if((arg==L"--camera"||arg==L"--look-at")&&i+3<argc){float values[3];for(float& v:values){size_t end;std::wstring text=argv[++i];v=std::stof(text,&end);if(end!=text.size()||!std::isfinite(v))throw std::runtime_error("Invalid camera coordinates");}if(arg==L"--camera"){explicit_camera={values[0],values[1],values[2]};has_camera=true;}else{explicit_target={values[0],values[1],values[2]};has_target=true;}}
            else if(arg==L"--view"&&i+1<argc){std::wstring view=argv[++i];if(view!=L"original"&&view!=L"overview")throw std::runtime_error("Expected --view original|overview");original_view=view==L"original";}
            else if(arg==L"--capture"&&i+1<argc)capture=fs::absolute(argv[++i]);
            else if(arg==L"--report"&&i+1<argc)report=fs::absolute(argv[++i]);
            else throw std::runtime_error("Usage: FS15EngineRebuilt --root PATH --map Map01|Map02 [--smoke] [--warp] [--capture PNG] [--report JSON]");
        }
        LocalFree(argv);if((reference_bytecode||test_pause||test_frame_rate!=60)&&!smoke)throw std::runtime_error("Reference and timing test options require --smoke");if(!smoke&&!scene_only)play=true;fs::create_directories(root/"logs");log_file.open(root/"logs"/("engine-"+map+".log"));
        if(!log_file)throw std::runtime_error("Cannot open engine log");
        log("ENGINE_BOOT authored Win32/D3D11 runtime; original engine not loaded");
        auto terrain=fs15world::load_terrain(root,map);
        log("I3D_READY nodes="+std::to_string(terrain.nodes.size())+" shape_nodes="+std::to_string(terrain.shape_nodes));
        camera.initialize(terrain,original_view);if(has_camera!=has_target)throw std::runtime_error("Use --camera and --look-at together");if(has_camera)camera.look_at(explicit_camera,explicit_target);auto home=camera.position;camera.move(10,0,0,1);bool camera_test=camera.position.x!=home.x||camera.position.y!=home.y||camera.position.z!=home.z;camera.reset();
        if(!camera_test)throw std::runtime_error("Camera movement check failed");
        if(play){mission=std::make_unique<MissionRuntime>(root,terrain,difficulty,reference_bytecode);if(!scenario.empty())mission->action(("scenario-"+scenario).c_str());XMFLOAT3 eye,target;if(mission->camera(eye,target))camera.look_at(eye,target);log("MISSION_READY vehicles="+std::to_string(mission->vehicle_count()));}
        if((reference_bytecode||test_pause)&&!mission)throw std::runtime_error("Reference/pause checks require a gameplay session");
        WNDCLASSW wc{};wc.hInstance=instance;wc.lpfnWndProc=window_proc;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.lpszClassName=L"FS15IndependentWorldRuntime";
        if(!RegisterClassW(&wc))throw std::runtime_error("Cannot register 3D window");
        DWORD style=WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX;RECT rect{0,0,int(width),int(height)};AdjustWindowRect(&rect,style,FALSE);
        std::wstring title=L"ReFarm - Farming Simulator 15 - "+wide(map)+L" | WASD/ZQSD, mouse look, Space/Ctrl, Home, Esc";
        window=CreateWindowW(wc.lpszClassName,title.c_str(),style,CW_USEDEFAULT,CW_USEDEFAULT,rect.right-rect.left,rect.bottom-rect.top,nullptr,nullptr,instance,nullptr);
        if(!window)throw std::runtime_error("Cannot create engine window");
        Renderer render(window,terrain,warp,root);if(!smoke)ShowWindow(window,show);
        if(test_pause){
            const auto before=mission->report();mission->action("pause");
            for(const char* action:{"next","attach","fill","tip","work","pipe"})mission->action(action);
            mission->update(120,1,1,false);mission->action("pause");
            if(mission->report()!=before)throw std::runtime_error("Paused gameplay changed or accumulated suspended time");
            keys['W']=true;mission_actions.push_back("fill");SendMessageW(window,WM_KILLFOCUS,0,0);
            if(!mission->paused()||keys['W']||!mission_actions.empty())throw std::runtime_error("Focus loss did not pause/clear input");
            mission->action("pause");if(mission->report()!=before)throw std::runtime_error("Focus pause changed gameplay state");
            log("MISSION_PAUSE_TEST_OK");
        }
        if(smoke&&scenario=="harvest"){for(unsigned i=0;i<5*test_frame_rate;++i)mission->update(1.0/test_frame_rate,1,0,false);XMFLOAT3 eye,target;if(mission->camera(eye,target))camera.look_at(eye,target);}
        if(smoke&&scenario=="tip"){for(unsigned i=0;i<4*test_frame_rate;++i)mission->update(1.0/test_frame_rate,0,0,true);XMFLOAT3 eye,target;if(mission->camera(eye,target))camera.look_at(eye,target);}
        if(mission_test){for(int i=0;i<300;++i)mission->update(1.0/60,1,0,false);mission->action("selfcheck");XMFLOAT3 eye,target;if(mission->camera(eye,target))camera.look_at(eye,target);}
        render.draw(true,!smoke);render.verify_visibility();
        auto vehicle_samples=mission?mission->visible_samples():0;if(mission&&!vehicle_samples)throw std::runtime_error("No original vehicle pixels rasterized");
        // Capture before Present: discard swap chains need not preserve content.
        if(!capture.empty()){render.draw(false,false);render.capture(capture);}
        if(report.empty())report=root/"logs"/("engine-"+map+".json");
        std::ofstream metadata(report);if(!metadata)throw std::runtime_error("Cannot write engine validation report");
        metadata<<"{\n\"scope\":\""<<(mission?"Recovered gameplay subset; original fleet/crops, approximate native services":"Original terrain and binary scene meshes; simplified materials, no physics")<<"\",\n"
            <<"\"map\":\""<<map<<"\",\n\"ready\":true,\n\"original_engine_loaded\":false,\n"
            <<"\"driver\":\""<<(render.software?"WARP":"hardware")<<"\",\n\"feature_level\":"<<render.feature_level<<",\n"
            <<"\"scene_nodes\":"<<terrain.nodes.size()<<",\n\"source_shape_nodes\":"<<terrain.shape_nodes<<",\n\"skipped_shape_nodes\":"<<terrain.skipped_instances<<",\n"
            <<"\"decoded_meshes\":"<<terrain.meshes.size()<<",\n\"rendered_instances\":"<<terrain.rendered_instances<<",\n\"object_triangles\":"<<terrain.object_triangles<<",\n"
            <<"\"terrain_layers\":"<<terrain.layers.size()<<",\n"
            <<"\"environment_ready\":"<<(render.environment_ready()?"true":"false")<<",\n\"original_shader_injections\":"<<render.original_injections()<<",\n\"animated_draws\":"<<render.animated_draws()<<",\n"
            <<"\"environment_hour\":"<<environment_hour<<",\n\"animation_time\":"<<animation_time<<",\n\"shadow_depth_pixels\":"<<render.shadow_pixels<<",\n\"shadow_resolution\":"<<render.shadow_resolution()<<",\n"
            <<"\"road_alignment_samples\":"<<terrain.road_alignment_samples<<",\n\"road_median_height_error\":"<<terrain.road_median_error<<",\n\"road_flipped_z_error\":"<<terrain.road_flipped_z_error<<",\n"
            <<"\"source_width\":"<<terrain.source_width<<",\n\"source_height\":"<<terrain.source_height<<",\n"
            <<"\"world_width\":"<<terrain.width<<",\n\"world_depth\":"<<terrain.depth<<",\n"
            <<"\"min_height\":"<<terrain.min_height<<",\n\"max_height\":"<<terrain.max_height<<",\n"
            <<"\"vertices\":"<<terrain.vertices.size()<<",\n\"triangles\":"<<terrain.indices.size()/3<<",\n"
            <<"\"visible_gpu_samples\":"<<render.samples<<",\n\"visible_object_gpu_samples\":"<<render.object_samples<<",\n\"camera_math_check\":true,\n\"first_instance_world\":[";
        if(!terrain.groups.empty())for(unsigned i=0;i<16;++i){if(i)metadata<<',';metadata<<terrain.groups[0].instances[0].value[i];}
        auto resources=terrain.used_resources;resources.insert(resources.end(),render.environment_resources().begin(),render.environment_resources().end());if(mission)resources.insert(resources.end(),mission->resources().begin(),mission->resources().end());std::sort(resources.begin(),resources.end());resources.erase(std::unique(resources.begin(),resources.end()),resources.end());
        metadata<<"],\n\"visible_vehicle_gpu_samples\":"<<vehicle_samples<<",\n\"mission\":"<<(mission?mission->report():"null")<<",\n\"resources\":[";for(size_t i=0;i<resources.size();++i){if(i)metadata<<',';metadata<<std::quoted(resources[i].generic_u8string());}
        metadata<<"]\n}\n";metadata.close();
        log(mission?"ENGINE_READY recovered gameplay subset running":"ENGINE_READY terrain geometry and texture rendered; free camera initialized");
        if(smoke){for(int frame=0;frame<8;++frame)render.draw();log("ENGINE_SMOKE_OK frames=9");}
        else {
            auto previous=std::chrono::steady_clock::now();MSG message{};
            while(!done){while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}if(done)break;
                auto now=std::chrono::steady_clock::now();double elapsed=std::chrono::duration<double>(now-previous).count();float dt=float(std::min(0.1,elapsed));previous=now;
                if(IsIconic(window)){if(mission)mission->set_paused(true);mission_actions.clear();Sleep(25);continue;}
                if(mission){
                    for(auto action:mission_actions)mission->action(action);mission_actions.clear();
                    mission->update(elapsed,float(keys['W']||keys['Z']||keys[VK_UP])-float(keys['S']||keys[VK_DOWN]),float(keys['A']||keys['Q']||keys[VK_LEFT])-float(keys['D']||keys[VK_RIGHT]),keys[VK_SPACE]);
                    XMFLOAT3 eye,target;follow_vehicle=mission->camera(eye,target);if(follow_vehicle)camera.look_at(eye,target);else camera.update(dt);
                    static float status_time=0;status_time+=dt;if(status_time>0.25f){auto status=mission->status();status=status.substr(0,status.find(L'\n'));SetWindowTextW(window,(L"ReFarm - "+status).c_str());status_time=0;}
                }
                else camera.update(dt);if(!mission||!mission->paused())animation_time+=dt;render.draw();
            }
        }
        if(mission){std::ofstream final_report(root/"logs"/("mission-"+map+"-final.json"));final_report<<mission->report()<<'\n';mission.reset();}
    }catch(const std::exception& error){log(std::string("FATAL ")+error.what());if(!smoke)MessageBoxW(window,wide(error.what()).c_str(),L"Reconstructed engine error",MB_OK|MB_ICONERROR);result=1;}
    mission.reset();if(window&&IsWindow(window))DestroyWindow(window);log("ENGINE_SHUTDOWN exit="+std::to_string(result));if(SUCCEEDED(com))CoUninitialize();return result;
}
