// Authored I3D metadata/terrain loader with reconstructed native TRS/affine math.
#include "resources.h"
#include "../core/scene_trs.h"
#include "../core/scene_math.h"
#include <windows.h>
#include <objidl.h>
#include <wincodec.h>
#include <xmllite.h>
#include <shlwapi.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <limits>
#include <DirectXMath.h>
#include <cctype>
#include <set>
#include <tuple>

namespace fs15world {
using Microsoft::WRL::ComPtr;
static void check(HRESULT result,const char* operation) {
    if(FAILED(result)) throw std::runtime_error(std::string(operation)+" failed: "+std::to_string(static_cast<unsigned long>(result)));
}
static std::string utf8(const wchar_t* value,UINT count) {
    if(!count)return {};
    int size=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value,count,nullptr,0,nullptr,nullptr);
    if(!size)throw std::runtime_error("Invalid I3D Unicode");
    std::string text(size,'\0');
    WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value,count,text.data(),size,nullptr,nullptr);
    return text;
}
static std::string name(IXmlReader* reader) {
    const wchar_t* text=nullptr;UINT count=0;check(reader->GetLocalName(&text,&count),"I3D name");return utf8(text,count);
}
using Attributes=std::map<std::string,std::string>;
static Attributes attributes(IXmlReader* reader) {
    Attributes out;HRESULT result=reader->MoveToFirstAttribute();check(result,"I3D attributes");
    if(result==S_OK) {
        do {auto key=name(reader);const wchar_t* text=nullptr;UINT count=0;
            check(reader->GetValue(&text,&count),"I3D attribute value");out.emplace(key,utf8(text,count));
            result=reader->MoveToNextAttribute();
        }while(result==S_OK);
        check(result,"I3D next attribute");check(reader->MoveToElement(),"I3D element");
    }
    return out;
}
static std::string value(const Attributes& a,const char* key,std::string fallback={}) {
    auto it=a.find(key);return it==a.end()?fallback:it->second;
}
static float number(const Attributes& a,const char* key,float fallback) {
    auto text=value(a,key);if(text.empty())return fallback;
    size_t end=0;float out=std::stof(text,&end);
    if(end!=text.size()||!std::isfinite(out))throw std::runtime_error(std::string("Invalid I3D number: ")+key);
    return out;
}
static std::array<float,3> vector3(const Attributes& a,const char* key,std::array<float,3> fallback={0,0,0}) {
    auto text=value(a,key);if(text.empty())return fallback;
    std::istringstream stream(text);std::array<double,3> precise{};std::array<float,3> out{};
    if(!(stream>>precise[0]>>precise[1]>>precise[2]))
        throw std::runtime_error(std::string("Invalid I3D vector ")+key+": "+text);
    for(size_t i=0;i<3;++i){if(!std::isfinite(precise[i])||std::abs(precise[i])>std::numeric_limits<float>::max())throw std::runtime_error("I3D vector outside float range");out[i]=static_cast<float>(precise[i]);}
    return out;
}
static std::filesystem::path resource(const std::filesystem::path& root,const std::filesystem::path& relative) {
    if(relative.is_absolute()||relative.has_root_name())throw std::runtime_error("Absolute I3D resource path");
    for(auto& part:relative)if(part=="..")throw std::runtime_error("I3D resource traversal");
    for(auto& prefix:{root/"assets",root/"recovered/gar"}) {
        auto path=prefix/relative;if(std::filesystem::is_regular_file(path))return path;
    }
    throw std::runtime_error("Missing prepared engine resource: "+relative.generic_u8string());
}
std::filesystem::path resolve_resource(const std::filesystem::path& root,const std::filesystem::path& relative){return resource(root,relative);}
std::string shader_injection(const std::filesystem::path& root,const std::filesystem::path& relative,const std::string& position){
    const auto path=resource(root,relative);if(std::filesystem::file_size(path)>1024*1024)throw std::runtime_error("Shader XML exceeds limit");
    ComPtr<IStream> input;ComPtr<IXmlReader> xml;
    check(SHCreateStreamOnFileEx(path.c_str(),STGM_READ|STGM_SHARE_DENY_WRITE,FILE_ATTRIBUTE_NORMAL,FALSE,nullptr,&input),"Shader input");
    check(CreateXmlReader(__uuidof(IXmlReader),reinterpret_cast<void**>(xml.GetAddressOf()),nullptr),"Shader XML");check(xml->SetProperty(XmlReaderProperty_DtdProcessing,DtdProcessing_Prohibit),"Shader DTD rejection");check(xml->SetInput(input.Get()),"Shader stream");
    bool active=false;std::string code;XmlNodeType kind;HRESULT result;
    while((result=xml->Read(&kind))==S_OK){
        if(kind==XmlNodeType_Element&&name(xml.Get())=="CodeInjection")active=value(attributes(xml.Get()),"position")==position;
        else if(kind==XmlNodeType_EndElement&&name(xml.Get())=="CodeInjection")active=false;
        else if(active&&(kind==XmlNodeType_Text||kind==XmlNodeType_CDATA||kind==XmlNodeType_Whitespace)){const wchar_t* text;UINT count;check(xml->GetValue(&text,&count),"Shader text");code+=utf8(text,count);}
    }
    check(result,"Shader parse");if(code.empty())throw std::runtime_error("Missing original shader injection: "+position);return code;
}
std::vector<Mesh> load_meshes(const std::filesystem::path& root,const std::filesystem::path& relative) {
    std::filesystem::path path;
    for(auto& prefix:{root/"assets",root/"recovered/gar"})if(std::filesystem::is_regular_file(prefix/relative))path=prefix/relative;
    if(path.empty())return {}; // Terrain-only packages and authored terrain fixtures.
    std::ifstream input(path,std::ios::binary);
    auto read=[&](void* data,size_t count){if(!input.read(static_cast<char*>(data),count))throw std::runtime_error("Truncated original mesh cache");};
    auto integer=[&](){std::uint32_t value;read(&value,4);return value;};
    char magic[8];read(magic,8);if(std::string(magic,8)!=std::string("FS15MSH\1",8))throw std::runtime_error("Invalid mesh cache version");
    unsigned count=integer();if(count>10000)throw std::runtime_error("Mesh cache count limit");
    std::vector<Mesh> meshes;meshes.reserve(count);
    for(unsigned i=0;i<count;++i){Mesh mesh;mesh.id=integer();unsigned nv=integer(),ni=integer(),ns=integer();
        if(nv>2000000||ni>6000000||ns>1000||ni%3)throw std::runtime_error("Invalid mesh buffer size");
        for(unsigned s=0;s<ns;++s){Subset part{integer(),integer()};if(part.first>ni||part.count>ni-part.first||part.count%3)throw std::runtime_error("Invalid original subset");mesh.subsets.push_back(part);}
        mesh.vertices.resize(nv);mesh.indices.resize(ni);read(mesh.vertices.data(),size_t(nv)*sizeof(Vertex));read(mesh.indices.data(),size_t(ni)*4);
        for(auto index:mesh.indices)if(index>=nv)throw std::runtime_error("Original mesh index outside vertices");
        for(const auto& v:mesh.vertices)for(float x:{v.x,v.y,v.z,v.nx,v.ny,v.nz,v.u,v.v})if(!std::isfinite(x))throw std::runtime_error("Non-finite mesh vertex");
        if(mesh.subsets.empty())mesh.subsets.push_back({0,ni});meshes.push_back(std::move(mesh));
    }
    if(input.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Trailing mesh cache bytes");return meshes;
}
static bool lower_lod(std::string text) {
    for(char& c:text)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    auto at=text.find("lod");return at!=std::string::npos&&at+3<text.size()&&text[at+3]>='1'&&text[at+3]<='9';
}
static ComPtr<IWICImagingFactory> wic() {
    ComPtr<IWICImagingFactory> factory;
    check(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)),"WIC factory");return factory;
}
static std::vector<std::uint16_t> height_pixels(const std::filesystem::path& path,unsigned& width,unsigned& height) {
    auto factory=wic();ComPtr<IWICBitmapDecoder> decoder;ComPtr<IWICBitmapFrameDecode> frame;
    check(factory->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&decoder),"Height PNG decoder");
    check(decoder->GetFrame(0,&frame),"Height PNG frame");check(frame->GetSize(&width,&height),"Height PNG size");
    if(width<3||height<3||width>4097||height>4097)throw std::runtime_error("Unsupported height map dimensions");
    WICPixelFormatGUID format;check(frame->GetPixelFormat(&format),"Height PNG pixel format");
    std::vector<std::uint16_t> out(size_t(width)*height);
    if(IsEqualGUID(format,GUID_WICPixelFormat16bppGray)) {
        check(frame->CopyPixels(nullptr,width*2,static_cast<UINT>(out.size()*2),reinterpret_cast<BYTE*>(out.data())),"16-bit height pixels");
    }else if(IsEqualGUID(format,GUID_WICPixelFormat24bppRGB)||IsEqualGUID(format,GUID_WICPixelFormat24bppBGR)) {
        std::vector<BYTE> rgb(size_t(width)*height*3);
        check(frame->CopyPixels(nullptr,width*3,static_cast<UINT>(rgb.size()),rgb.data()),"Packed RG height pixels");
        // The local FS15 base maps are RGB8: R is the high byte, G the low
        // byte, and B is zero. Preserve all 16 bits rather than gray conversion.
        const unsigned red=IsEqualGUID(format,GUID_WICPixelFormat24bppBGR)?2:0,blue=2-red;
        for(size_t i=0;i<out.size();++i) {
            if(rgb[i*3+blue])throw std::runtime_error("Unsupported RGB DEM encoding (B must be zero)");
            out[i]=static_cast<std::uint16_t>((rgb[i*3+red]<<8)|rgb[i*3+1]);
        }
    }else throw std::runtime_error("Unsupported DEM PNG format; expected packed RG16 or Gray16");
    return out;
}
WeightImage load_weight(const std::filesystem::path& filename) {
    auto factory=wic();ComPtr<IWICBitmapDecoder> decoder;ComPtr<IWICBitmapFrameDecode> frame;ComPtr<IWICFormatConverter> converter;WeightImage image;
    check(factory->CreateDecoderFromFilename(filename.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&decoder),"Weight decoder");
    check(decoder->GetFrame(0,&frame),"Weight frame");check(frame->GetSize(&image.width,&image.height),"Weight dimensions");
    if(!image.width||!image.height||image.width>4096||image.height>4096)throw std::runtime_error("Invalid terrain weight dimensions");
    check(factory->CreateFormatConverter(&converter),"Weight converter");check(converter->Initialize(frame.Get(),GUID_WICPixelFormat8bppGray,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom),"Weight gray conversion");
    image.pixels.resize(size_t(image.width)*image.height);check(converter->CopyPixels(nullptr,image.width,static_cast<UINT>(image.pixels.size()),image.pixels.data()),"Weight pixels");return image;
}
static Terrain load_scene_impl(const std::filesystem::path& root,const std::string& map_id,const std::filesystem::path& relative,bool require_terrain,bool dynamic_visibility=false) {
    Terrain out;out.map_id=map_id;
    const auto stem=map_id=="Map01"?"map01":"map02";
    out.scene_file=resource(root,relative);
    auto mesh_relative=relative;mesh_relative.replace_extension(".meshes");out.meshes=load_meshes(root,mesh_relative);
    std::map<unsigned,unsigned> mesh_ids;for(unsigned i=0;i<out.meshes.size();++i)if(!mesh_ids.emplace(out.meshes[i].id,i).second)throw std::runtime_error("Duplicate mesh ID");
    std::map<std::tuple<unsigned,std::vector<int>,bool,bool>,unsigned> group_ids;
    ComPtr<IStream> input;ComPtr<IXmlReader> xml;
    check(SHCreateStreamOnFileEx(out.scene_file.c_str(),STGM_READ|STGM_SHARE_DENY_WRITE,FILE_ATTRIBUTE_NORMAL,FALSE,nullptr,&input),"Open I3D");
    check(CreateXmlReader(__uuidof(IXmlReader),reinterpret_cast<void**>(xml.GetAddressOf()),nullptr),"I3D reader");
    check(xml->SetProperty(XmlReaderProperty_DtdProcessing,DtdProcessing_Prohibit),"Reject I3D DTD");
    check(xml->SetProperty(XmlReaderProperty_MaxElementDepth,512),"I3D depth limit");check(xml->SetInput(input.Get()),"I3D stream");
    std::map<int,std::string> files;std::vector<std::pair<UINT,int>> parents;
    bool scene=false,terrain_found=false;int height_id=0,current_material=-1,user_node=-1;UINT scene_depth=0,material_depth=0;
    std::map<int,int> node_ids;
    XmlNodeType kind;HRESULT result;
    while((result=xml->Read(&kind))==S_OK) {
        UINT depth=0;xml->GetDepth(&depth);
        if(kind==XmlNodeType_EndElement) {
            if(current_material!=-1&&depth==material_depth)current_material=-1;
            if(scene&&depth==scene_depth)scene=false;
            while(!parents.empty()&&parents.back().first>=depth)parents.pop_back();
            continue;
        }
        if(kind!=XmlNodeType_Element)continue;
        const auto tag=name(xml.Get());
        if(tag=="FoliageSubLayer"){
            auto a=attributes(xml.Get());FoliageLayer layer;layer.name=value(a,"name");layer.type=unsigned(number(a,"densityMapTypeIndex",0));layer.offset=unsigned(number(a,"densityMapChannelOffset",0));layer.channels=unsigned(number(a,"numDensityMapChannels",0));layer.material=std::stoi(value(a,"materialId","0"));layer.blocks=number(a,"numBlocksPerUnit",1);
            auto list=[&](const char* key){std::vector<float> out;std::istringstream stream(value(a,key));std::string part;while(std::getline(stream,part,';'))out.push_back(part.empty()?0:std::stof(part));return out;};
            auto shapes=list("blockShapeId");for(float v:shapes)layer.shapes.push_back(unsigned(v));layer.widths=list("width");layer.heights=list("height");layer.states=unsigned(number(a,"numStates",1));
            if(layer.type&&layer.offset<16&&layer.channels<=8)out.foliage.push_back(std::move(layer));continue;
        }
        if(tag=="UserAttribute"){auto a=attributes(xml.Get());auto at=node_ids.find(std::stoi(value(a,"nodeId","0")));user_node=at==node_ids.end()?-1:at->second;continue;}
        if(tag=="Attribute"&&user_node>=0){auto a=attributes(xml.Get());out.nodes[user_node].attributes[value(a,"name")]=value(a,"value");out.nodes[user_node].attribute_types[value(a,"name")]=value(a,"type","string");continue;}
        if(tag=="Scene"){scene=true;scene_depth=depth;continue;}
        if(tag=="File") {
            auto a=attributes(xml.Get());files.emplace(std::stoi(value(a,"fileId")),value(a,"filename"));continue;
        }
        if(tag=="Material") {
            auto a=attributes(xml.Get());current_material=std::stoi(value(a,"materialId"));material_depth=depth;
            Material material;auto color=vector3(a,"diffuseColor",{1,1,1});std::copy(color.begin(),color.end(),material.color);
            material.alpha=value(a,"alphaBlending")=="true"||value(a,"alphaTest")=="true";
            auto shader_file=files.find(std::stoi(value(a,"customShaderId","0")));
            material.water=shader_file!=files.end()&&(shader_file->second.find("oceanShader")!=std::string::npos||shader_file->second.find("waterShader")!=std::string::npos);
            if(shader_file!=files.end()){
                auto path=(relative.parent_path()/std::filesystem::u8path(shader_file->second)).lexically_normal();auto leaf=path.filename().u8string();
                material.effect=leaf=="windShader.xml"?1:leaf=="flagShader.xml"?2:leaf=="treeBranchShader.xml"?3:0;
                if(material.effect)material.original_shader=path;
            }
            out.materials.emplace(current_material,std::move(material));if(xml->IsEmptyElement())current_material=-1;continue;
        }
        if(tag=="Texture"&&current_material!=-1&&depth==material_depth+1) {
            auto a=attributes(xml.Get());auto file=files.find(std::stoi(value(a,"fileId","0")));
            if(file!=files.end()&&!file->second.empty()){
                auto filename=std::filesystem::u8path(file->second);auto requested=relative.parent_path()/filename;
                if(file->second.rfind("$data/",0)==0)requested=std::filesystem::u8path(file->second.substr(1));
                requested=requested.lexically_normal();requested.replace_extension(".dds");
                // Resolve textures only when a mesh actually uses this material.
                out.materials[current_material].texture=requested;
            }continue;
        }
        if(tag=="CustomParameter"&&current_material!=-1){auto a=attributes(xml.Get());
            if(value(a,"name")=="directionAndFrequency"||value(a,"name")=="displacementSpeedFrequencyAndTexOff"){
                std::istringstream stream(value(a,"value"));for(float& v:out.materials[current_material].effect_params)if(!(stream>>v)||!std::isfinite(v))throw std::runtime_error("Invalid original animation parameters");
            }
            if(out.materials[current_material].water&&value(a,"name")=="underwaterFogColor"){
                auto color=vector3(a,"value");std::copy(color.begin(),color.end(),out.materials[current_material].color);
            }continue;
        }
        if(tag=="Layer"&&scene&&terrain_found){auto a=attributes(xml.Get());Layer layer;auto detail=files.find(std::stoi(value(a,"detailMapId","0"))),weight=files.find(std::stoi(value(a,"weightMapId","0")));
            if(detail!=files.end()&&weight!=files.end()){
                layer.detail=(relative.parent_path()/std::filesystem::u8path(detail->second)).lexically_normal();layer.detail.replace_extension(".dds");
                layer.weight=(relative.parent_path()/std::filesystem::u8path(weight->second)).lexically_normal();layer.unit_size=number(a,"unitSize",1);
                if(layer.unit_size<=0||out.layers.size()>=8)throw std::runtime_error("Unsupported terrain layer definition");
                out.layers.push_back(std::move(layer));
            }continue;
        }
        if(!scene)continue;
        if(tag!="TransformGroup"&&tag!="TerrainTransformGroup"&&tag!="Shape"&&tag!="Camera"&&tag!="Light"&&tag!="ReferenceNode")continue;
        if(out.nodes.size()>=500000)throw std::runtime_error("I3D scene exceeds node limit");
        while(!parents.empty()&&parents.back().first>=depth)parents.pop_back();
        auto a=attributes(xml.Get());Node node;node.name=value(a,"name");node.type=tag;
        if(tag=="Light"&&value(a,"type")=="directional"){
            out.sun.shadows=value(a,"castShadowMap")=="true";out.sun.resolution=static_cast<unsigned>(number(a,"depthMapResolution",512));
            out.sun.distance=number(a,"shadowFarDistance",80);out.sun.bias=number(a,"depthMapBias",0.0012f);out.sun.slope=number(a,"depthMapSlopeScaleBias",0.0005f);out.sun.extrusion=number(a,"shadowExtrusionDistance",200);
            if(out.sun.resolution<64||out.sun.resolution>4096||out.sun.distance<=0||out.sun.extrusion<=0)throw std::runtime_error("Unsupported original sun shadow settings");
        }
        node.id=std::stoi(value(a,"nodeId","0"));node.parent=parents.empty()?-1:parents.back().second;
        node.collidable=tag=="Shape"&&(value(a,"static")=="true"||value(a,"dynamic")=="true")&&value(a,"trigger")!="true"&&value(a,"collisionMask","1")!="0";
        auto translation=vector3(a,"translation");std::copy(translation.begin(),translation.end(),node.translation);
        auto rotation=vector3(a,"rotation"),scale=vector3(a,"scale",{1,1,1});
        for(int k=0;k<3;++k){node.rotation[k]=DirectX::XMConvertToRadians(rotation[k]);node.scale[k]=scale[k];}
        using namespace DirectX;
        float local[16];
        if(!fs15_compose_trs(node.rotation,node.scale,node.translation,local,16))throw std::runtime_error("Recovered I3D TRS composition failed");
        if(node.parent>=0){if(!fs15_affine_multiply(out.nodes[node.parent].world,16,local,16,node.world,16))throw std::runtime_error("Recovered I3D parent composition failed");}
        else std::copy(local,local+16,node.world);
        node.local_visible=value(a,"visibility","true")!="false";
        node.lod_visible=!lower_lod(node.name)&&(node.parent<0||out.nodes[node.parent].lod_visible);
        node.visible=node.local_visible&&node.lod_visible&&(node.parent<0||out.nodes[node.parent].visible);
        const auto index=static_cast<int>(out.nodes.size());out.nodes.push_back(std::move(node));
        node_ids[out.nodes.back().id]=index;if(out.nodes.back().parent>=0)out.nodes[out.nodes.back().parent].children.push_back(index);
        if(!xml->IsEmptyElement())parents.emplace_back(depth,index);
        if(tag=="Shape"){
            ++out.shape_nodes;auto mesh=mesh_ids.find(static_cast<unsigned>(std::stoul(value(a,"shapeId","0"))));
            if(mesh!=mesh_ids.end())out.nodes.back().mesh=static_cast<int>(mesh->second);
            if(mesh!=mesh_ids.end()&&(out.nodes.back().visible||(dynamic_visibility&&out.nodes.back().lod_visible))&&value(a,"nonRenderable")!="true"&&value(a,"trigger")!="true"&&!value(a,"materialIds").empty()){
                std::istringstream stream(value(a,"materialIds"));std::vector<int> material_ids;int id;while(stream>>id)material_ids.push_back(id);
                bool casts=value(a,"castsShadows","false")=="true",receives=value(a,"receiveShadows","true")!="false";
                const auto key=std::make_tuple(mesh->second,material_ids,casts,receives);auto group=group_ids.find(key);unsigned group_index;
                if(group==group_ids.end()){group_index=static_cast<unsigned>(out.groups.size());group_ids.emplace(key,group_index);out.groups.push_back({mesh->second,material_ids,{},casts,receives});}else group_index=group->second;
                Matrix matrix;std::copy(out.nodes.back().world,out.nodes.back().world+16,matrix.value);out.groups[group_index].instances.push_back(matrix);
                out.groups[group_index].nodes.push_back(index);
                ++out.rendered_instances;out.object_triangles+=out.meshes[mesh->second].indices.size()/3;
            }else ++out.skipped_instances;
        }
        if(tag=="TerrainTransformGroup") {
            if(terrain_found||out.nodes.back().parent!=-1||translation!=std::array<float,3>{0,0,0}
                ||vector3(a,"rotation")!=std::array<float,3>{0,0,0}||vector3(a,"scale",{1,1,1})!=std::array<float,3>{1,1,1})
                throw std::runtime_error("Only one untransformed root terrain is supported");
            terrain_found=true;height_id=std::stoi(value(a,"heightMapId"));
            out.height_scale=number(a,"heightScale",255);out.units_per_pixel=number(a,"unitsPerPixel",1);
        }
    }
    check(result,"Parse I3D");
    if(require_terrain&&(!terrain_found||!files.count(height_id)||out.height_scale<=0||out.units_per_pixel<=0))throw std::runtime_error("I3D terrain definition is incomplete");
    out.file_references=static_cast<unsigned>(files.size());
    std::set<std::filesystem::path> used{relative};
    if(terrain_found){
    out.height_file=resource(root,relative.parent_path()/std::filesystem::u8path(files.at(height_id)));
    out.texture_file=resource(root,relative.parent_path()/stem/"pda_map_H.dds");
    auto heights=height_pixels(out.height_file,out.source_width,out.source_height);
    out.heights=heights;
    out.width=(out.source_width-1)*out.units_per_pixel;out.depth=(out.source_height-1)*out.units_per_pixel;
    auto extrema=std::minmax_element(heights.begin(),heights.end());
    out.min_height=*extrema.first*out.height_scale/65535.0f;out.max_height=*extrema.second*out.height_scale/65535.0f;
    // Fixed two-source-pixel spacing bounds GPU buffers without inventing relief.
    const unsigned step=2,nx=(out.source_width-1)/step+1,nz=(out.source_height-1)/step+1;
    if((out.source_width-1)%step||(out.source_height-1)%step)throw std::runtime_error("DEM grid must have an even interval count");
    auto sample=[&](int x,int z){x=std::clamp(x,0,int(out.source_width)-1);z=std::clamp(z,0,int(out.source_height)-1);return heights[size_t(z)*out.source_width+x]*out.height_scale/65535.0f;};
    std::vector<float> road_errors,flipped_errors;
    auto bilinear=[&](float x,float z){float px=x/out.units_per_pixel+(out.source_width-1)*0.5f,pz=z/out.units_per_pixel+(out.source_height-1)*0.5f;
        int ix=static_cast<int>(std::floor(px)),iz=static_cast<int>(std::floor(pz));float a=px-ix,b=pz-iz;
        return (1-b)*((1-a)*sample(ix,iz)+a*sample(ix+1,iz))+b*((1-a)*sample(ix,iz+1)+a*sample(ix+1,iz+1));};
    for(const auto& node:out.nodes)if(node.mesh>=0&&node.visible&&(node.name.find("mainRoad")!=std::string::npos||node.name.find("roadShape")!=std::string::npos)){
        const auto& mesh=out.meshes[node.mesh];DirectX::XMFLOAT4X4 w;std::copy(node.world,node.world+16,&w._11);auto matrix=DirectX::XMLoadFloat4x4(&w);
        for(size_t i=0;i<mesh.vertices.size();i+=std::max(size_t(1),mesh.vertices.size()/32)){
            const auto& v=mesh.vertices[i];DirectX::XMFLOAT3 p;DirectX::XMStoreFloat3(&p,DirectX::XMVector3TransformCoord(DirectX::XMVectorSet(v.x,v.y,v.z,1),matrix));
            if(std::abs(p.x)>=out.width/2||std::abs(p.z)>=out.depth/2)continue;
            road_errors.push_back(std::abs(p.y-bilinear(p.x,p.z)));flipped_errors.push_back(std::abs(p.y-bilinear(p.x,-p.z)));
        }
    }
    if(!road_errors.empty()){out.road_alignment_samples=static_cast<unsigned>(road_errors.size());std::sort(road_errors.begin(),road_errors.end());std::sort(flipped_errors.begin(),flipped_errors.end());out.road_median_error=road_errors[road_errors.size()/2];out.road_flipped_z_error=flipped_errors[flipped_errors.size()/2];}
    out.vertices.reserve(size_t(nx)*nz);
    for(unsigned z=0;z<nz;++z)for(unsigned x=0;x<nx;++x) {
        const int px=x*step,pz=z*step;float dx=sample(px+step,pz)-sample(px-step,pz),dz=sample(px,pz+step)-sample(px,pz-step);
        float ny=2*step*out.units_per_pixel,length=std::sqrt(dx*dx+ny*ny+dz*dz);
        float u=float(px)/(out.source_width-1),v=float(pz)/(out.source_height-1);
        out.vertices.push_back({(u-0.5f)*out.width,sample(px,pz),(v-0.5f)*out.depth,-dx/length,ny/length,-dz/length,u,v});
    }
    out.indices.reserve(size_t(nx-1)*(nz-1)*6);
    for(unsigned z=0;z<nz-1;++z)for(unsigned x=0;x<nx-1;++x) {
        unsigned a=z*nx+x,b=a+1,c=a+nx,d=c+1;out.indices.insert(out.indices.end(),{a,c,b,b,c,d});
    }
    used.insert((relative.parent_path()/std::filesystem::u8path(files.at(height_id))).lexically_normal());used.insert(relative.parent_path()/stem/"pda_map_H.dds");
    }
    if(!out.meshes.empty())used.insert(mesh_relative);
    for(const auto& group:out.groups)for(int id:group.materials){auto found=out.materials.find(id);if(found!=out.materials.end()){if(!found->second.texture.empty())used.insert(found->second.texture);if(!found->second.original_shader.empty())used.insert(found->second.original_shader);}}
    for(const auto& layer:out.layers){used.insert(layer.detail);used.insert(layer.weight);}
    out.used_resources.assign(used.begin(),used.end());
    return out;
}
Terrain load_terrain(const std::filesystem::path& root,const std::string& map_id){
    if(map_id!="Map01"&&map_id!="Map02")throw std::runtime_error("Only Map01 and Map02 are currently supported");
    return load_scene_impl(root,map_id,std::filesystem::path("data/maps")/(map_id=="Map01"?"map01.i3d":"map02.i3d"),true);
}
Terrain load_scene(const std::filesystem::path& root,const std::filesystem::path& relative,bool dynamic_visibility){return load_scene_impl(root,"",relative,false,dynamic_visibility);}
void update_transforms(Terrain& scene,const Matrix& root){
    for(auto& node:scene.nodes){float local[16];
        if(!fs15_compose_trs(node.rotation,node.scale,node.translation,local,16))throw std::runtime_error("Recovered animated TRS composition failed");
        const float* parent=node.parent>=0?scene.nodes[node.parent].world:root.value;
        if(!fs15_affine_multiply(parent,16,local,16,node.world,16))throw std::runtime_error("Recovered animated hierarchy composition failed");
    }
    update_visibility(scene);
}
void update_visibility(Terrain& scene){
    for(auto& node:scene.nodes)node.visible=node.local_visible&&node.lod_visible&&(node.parent<0||scene.nodes[node.parent].visible);
    for(auto& group:scene.groups){group.instances.clear();for(int index:group.nodes)if(scene.nodes[index].visible){Matrix matrix;std::copy(scene.nodes[index].world,scene.nodes[index].world+16,matrix.value);group.instances.push_back(matrix);}}
}
float terrain_height(const Terrain& t,float x,float z){
    if(t.heights.empty())throw std::runtime_error("Terrain height service unavailable");
    const float px=std::clamp(x/t.units_per_pixel+(t.source_width-1)*0.5f,0.0f,float(t.source_width-1)),pz=std::clamp(z/t.units_per_pixel+(t.source_height-1)*0.5f,0.0f,float(t.source_height-1));
    const unsigned ix=unsigned(px),iz=unsigned(pz);const float a=px-ix,b=pz-iz;
    auto at=[&](unsigned xx,unsigned zz){return t.heights[size_t(std::min(zz,t.source_height-1))*t.source_width+std::min(xx,t.source_width-1)]*t.height_scale/65535.0f;};
    return (1-b)*((1-a)*at(ix,iz)+a*at(ix+1,iz))+b*((1-a)*at(ix,iz+1)+a*at(ix+1,iz+1));
}
static std::uint32_t u32(const std::vector<std::uint8_t>& bytes,size_t offset) {
    if(offset+4>bytes.size())throw std::runtime_error("Truncated DDS header");
    return bytes[offset]|(bytes[offset+1]<<8)|(bytes[offset+2]<<16)|(std::uint32_t(bytes[offset+3])<<24);
}
Texture load_dds(const std::filesystem::path& path) {
    std::ifstream file(path,std::ios::binary);if(!file)throw std::runtime_error("Missing terrain overview DDS");
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)),{});
    if(bytes.size()<128||u32(bytes,0)!=0x20534444||u32(bytes,4)!=124||u32(bytes,76)!=32)throw std::runtime_error("Invalid terrain DDS");
    Texture out;out.width=u32(bytes,16);out.height=u32(bytes,12);auto fourcc=u32(bytes,84);
    if(!out.width||!out.height||out.width>8192||out.height>8192)throw std::runtime_error("Unsupported terrain texture dimensions");
    if(fourcc==0x31545844)out.compression=1;else if(fourcc==0x33545844)out.compression=2;else if(fourcc==0x35545844)out.compression=3;
    else throw std::runtime_error("Terrain overview must be DXT1/DXT3/DXT5");
    unsigned mip_count=std::max(1u,u32(bytes,28)),w=out.width,h=out.height;size_t size=0;
    if(mip_count>14)throw std::runtime_error("Invalid DDS mip count");
    for(unsigned mip=0;mip<mip_count;++mip) {
        unsigned pitch=((w+3)/4)*(out.compression==1?8:16);
        out.mip_offsets.push_back(static_cast<unsigned>(size));out.mip_pitches.push_back(pitch);
        size+=size_t(pitch)*((h+3)/4);w=std::max(1u,w/2);h=std::max(1u,h/2);
    }
    out.row_pitch=out.mip_pitches[0];
    if(bytes.size()<128+size)throw std::runtime_error("Truncated terrain DDS mip");
    out.bytes.assign(bytes.begin()+128,bytes.begin()+128+size);return out;
}
void save_png(const std::filesystem::path& path,unsigned width,unsigned height,const std::uint8_t* bgra,unsigned pitch) {
    auto factory=wic();ComPtr<IWICStream> stream;ComPtr<IWICBitmapEncoder> encoder;ComPtr<IWICBitmapFrameEncode> frame;
    check(factory->CreateStream(&stream),"PNG stream");check(stream->InitializeFromFilename(path.c_str(),GENERIC_WRITE),"PNG output");
    check(factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder),"PNG encoder");check(encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache),"PNG init");
    check(encoder->CreateNewFrame(&frame,nullptr),"PNG frame");check(frame->Initialize(nullptr),"PNG frame init");check(frame->SetSize(width,height),"PNG size");
    auto format=GUID_WICPixelFormat32bppBGRA;check(frame->SetPixelFormat(&format),"PNG format");
    if(!IsEqualGUID(format,GUID_WICPixelFormat32bppBGRA))throw std::runtime_error("PNG output format changed");
    check(frame->WritePixels(height,pitch,pitch*height,const_cast<BYTE*>(bgra)),"PNG pixels");check(frame->Commit(),"PNG frame commit");check(encoder->Commit(),"PNG commit");
}
}
