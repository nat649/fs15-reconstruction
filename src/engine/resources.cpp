// Authored I3D metadata/terrain resource loader. No original engine code is used.
#include "resources.h"
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
    std::istringstream stream(text);std::array<float,3> out{};
    if(!(stream>>out[0]>>out[1]>>out[2])||!std::isfinite(out[0])||!std::isfinite(out[1])||!std::isfinite(out[2]))
        throw std::runtime_error("Invalid I3D vector");
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
Terrain load_terrain(const std::filesystem::path& root,const std::string& map_id) {
    if(map_id!="Map01"&&map_id!="Map02")throw std::runtime_error("Only Map01 and Map02 are currently supported");
    Terrain out;out.map_id=map_id;
    const auto stem=map_id=="Map01"?"map01":"map02";
    const auto relative=std::filesystem::path("data/maps")/(std::string(stem)+".i3d");
    out.scene_file=resource(root,relative);
    ComPtr<IStream> input;ComPtr<IXmlReader> xml;
    check(SHCreateStreamOnFileEx(out.scene_file.c_str(),STGM_READ|STGM_SHARE_DENY_WRITE,FILE_ATTRIBUTE_NORMAL,FALSE,nullptr,&input),"Open I3D");
    check(CreateXmlReader(__uuidof(IXmlReader),reinterpret_cast<void**>(xml.GetAddressOf()),nullptr),"I3D reader");
    check(xml->SetProperty(XmlReaderProperty_DtdProcessing,DtdProcessing_Prohibit),"Reject I3D DTD");
    check(xml->SetProperty(XmlReaderProperty_MaxElementDepth,512),"I3D depth limit");check(xml->SetInput(input.Get()),"I3D stream");
    std::map<int,std::string> files;std::vector<std::pair<UINT,int>> parents;
    bool scene=false,terrain_found=false;int height_id=0;UINT scene_depth=0;
    XmlNodeType kind;HRESULT result;
    while((result=xml->Read(&kind))==S_OK) {
        UINT depth=0;xml->GetDepth(&depth);
        if(kind==XmlNodeType_EndElement) {
            if(scene&&depth==scene_depth)scene=false;
            while(!parents.empty()&&parents.back().first>=depth)parents.pop_back();
            continue;
        }
        if(kind!=XmlNodeType_Element)continue;
        const auto tag=name(xml.Get());
        if(tag=="Scene"){scene=true;scene_depth=depth;continue;}
        if(tag=="File") {
            auto a=attributes(xml.Get());files.emplace(std::stoi(value(a,"fileId")),value(a,"filename"));continue;
        }
        if(!scene)continue;
        if(tag!="TransformGroup"&&tag!="TerrainTransformGroup"&&tag!="Shape"&&tag!="Camera"&&tag!="Light"&&tag!="ReferenceNode")continue;
        if(out.nodes.size()>=500000)throw std::runtime_error("I3D scene exceeds node limit");
        while(!parents.empty()&&parents.back().first>=depth)parents.pop_back();
        auto a=attributes(xml.Get());Node node;node.name=value(a,"name");node.type=tag;
        node.id=std::stoi(value(a,"nodeId","0"));node.parent=parents.empty()?-1:parents.back().second;
        auto translation=vector3(a,"translation");std::copy(translation.begin(),translation.end(),node.translation);
        const auto index=static_cast<int>(out.nodes.size());out.nodes.push_back(std::move(node));
        if(!xml->IsEmptyElement())parents.emplace_back(depth,index);
        if(tag=="Shape")++out.deferred_shapes;
        if(tag=="TerrainTransformGroup") {
            if(terrain_found||out.nodes.back().parent!=-1||translation!=std::array<float,3>{0,0,0}
                ||vector3(a,"rotation")!=std::array<float,3>{0,0,0}||vector3(a,"scale",{1,1,1})!=std::array<float,3>{1,1,1})
                throw std::runtime_error("Only one untransformed root terrain is supported");
            terrain_found=true;height_id=std::stoi(value(a,"heightMapId"));
            out.height_scale=number(a,"heightScale",255);out.units_per_pixel=number(a,"unitsPerPixel",1);
        }
    }
    check(result,"Parse I3D");
    if(!terrain_found||!files.count(height_id)||out.height_scale<=0||out.units_per_pixel<=0)throw std::runtime_error("I3D terrain definition is incomplete");
    out.file_references=static_cast<unsigned>(files.size());
    out.height_file=resource(root,relative.parent_path()/std::filesystem::u8path(files.at(height_id)));
    out.texture_file=resource(root,relative.parent_path()/stem/"pda_map_H.dds");
    auto heights=height_pixels(out.height_file,out.source_width,out.source_height);
    out.width=(out.source_width-1)*out.units_per_pixel;out.depth=(out.source_height-1)*out.units_per_pixel;
    auto extrema=std::minmax_element(heights.begin(),heights.end());
    out.min_height=*extrema.first*out.height_scale/65535.0f;out.max_height=*extrema.second*out.height_scale/65535.0f;
    // Fixed two-source-pixel spacing bounds GPU buffers without inventing relief.
    const unsigned step=2,nx=(out.source_width-1)/step+1,nz=(out.source_height-1)/step+1;
    if((out.source_width-1)%step||(out.source_height-1)%step)throw std::runtime_error("DEM grid must have an even interval count");
    auto sample=[&](int x,int z){x=std::clamp(x,0,int(out.source_width)-1);z=std::clamp(z,0,int(out.source_height)-1);return heights[size_t(z)*out.source_width+x]*out.height_scale/65535.0f;};
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
    return out;
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
