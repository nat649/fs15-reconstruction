#include "render.h"
#include "file_transport.h"
#ifdef _WIN32
#include <objidl.h>
#include <gdiplus.h>
#else
#include <SDL.h>
#include <SDL_image.h>
#endif
extern "C" {
#include <lua.h>
#include <lauxlib.h>
}
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cwchar>
#include <cwctype>
#include <cstring>
#include <fstream>
#include <map>
#include <stdexcept>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace fs15menu {
namespace {
#ifdef _WIN32
using namespace Gdiplus;
#endif
using Bytes = std::vector<std::uint8_t>;
Bytes read_bytes(const std::filesystem::path& path) {
    return fs15platform::read_file_bounded(path,512*1024*1024);
}
std::uint32_t little32(const Bytes& b, std::size_t p) {
    if (p + 4 > b.size()) throw std::runtime_error("Truncated DDS header");
    return b[p] | std::uint32_t(b[p+1])<<8 | std::uint32_t(b[p+2])<<16 | std::uint32_t(b[p+3])<<24;
}
std::uint32_t big32(const Bytes& b, std::size_t p) {
    if (p + 4 > b.size()) throw std::runtime_error("Truncated font record");
    return std::uint32_t(b[p])<<24 | std::uint32_t(b[p+1])<<16 | std::uint32_t(b[p+2])<<8 | b[p+3];
}
float big_float(const Bytes& b, std::size_t p) {
    auto value = big32(b,p); float result; std::memcpy(&result, &value, 4); return result;
}
std::uint32_t rgb565(unsigned c) {
    unsigned r=(c>>11)&31, g=(c>>5)&63, b=c&31;
    return 0xff000000u | ((r<<3)|(r>>2))<<16 | ((g<<2)|(g>>4))<<8 | (b<<3)|(b>>2);
}
std::uint32_t interpolate_rgb(std::uint32_t a, std::uint32_t b, unsigned wa, unsigned wb, unsigned divisor) {
    std::uint32_t result=0xff000000u;
    for (unsigned shift : {0u,8u,16u}) result |= ((((a>>shift)&255)*wa+((b>>shift)&255)*wb)/divisor)<<shift;
    return result;
}
struct Texture {
    int width=0, height=0;
    bool opaque=false;
    std::vector<std::uint32_t> pixels;
#ifdef _WIN32
    std::unique_ptr<Bitmap> bitmap;
#else
    SDL_Texture* bitmap=nullptr;
    ~Texture(){if(bitmap)SDL_DestroyTexture(bitmap);}
#endif
    void finish() {
        opaque=std::all_of(pixels.begin(),pixels.end(),[](std::uint32_t value){return(value>>24)==255;});
        #ifdef _WIN32
        bitmap=std::make_unique<Bitmap>(width,height,width*4,PixelFormat32bppARGB,reinterpret_cast<BYTE*>(pixels.data()));
        if (bitmap->GetLastStatus()!=Ok) throw std::runtime_error("Could not create decoded bitmap");
        #endif
    }
};
std::shared_ptr<Texture> decode_dds(const std::filesystem::path& path) {
    const Bytes b=read_bytes(path);
    if (b.size()<128 || little32(b,0)!=0x20534444 || little32(b,4)!=124 || little32(b,76)!=32)
        throw std::runtime_error("Invalid DDS: "+path.u8string());
    auto t=std::make_shared<Texture>();
    const auto width=little32(b,16), height=little32(b,12);
    if (!width || !height || width>16384 || height>16384 || std::uint64_t(width)*height>67108864)
        throw std::runtime_error("Unsupported DDS dimensions");
    t->width=static_cast<int>(width); t->height=static_cast<int>(height); t->pixels.resize(std::size_t(width)*height);
    const auto flags=little32(b,80), fourcc=little32(b,84);
    if (flags&4) {
        const bool bc1=fourcc==0x31545844, bc2=fourcc==0x33545844, bc3=fourcc==0x35545844;
        if (!bc1&&!bc2&&!bc3) throw std::runtime_error("DDS codec unsupported (need DXT1, DXT3 or DXT5)");
        const unsigned blockBytes=bc1?8:16, blocksX=(width+3)/4, blocksY=(height+3)/4;
        if (128ull+std::uint64_t(blocksX)*blocksY*blockBytes>b.size()) throw std::runtime_error("Truncated DDS blocks");
        for (unsigned by=0;by<blocksY;++by) for(unsigned bx=0;bx<blocksX;++bx) {
            const std::size_t start=128+(std::size_t(by)*blocksX+bx)*blockBytes, cpos=start+(bc1?0:8);
            const unsigned c0=b[cpos]|unsigned(b[cpos+1])<<8, c1=b[cpos+2]|unsigned(b[cpos+3])<<8;
            std::array<std::uint32_t,4> colors{rgb565(c0),rgb565(c1),0,0};
            if (c0>c1 || !bc1) { colors[2]=interpolate_rgb(colors[0],colors[1],2,1,3); colors[3]=interpolate_rgb(colors[0],colors[1],1,2,3); }
            else { colors[2]=interpolate_rgb(colors[0],colors[1],1,1,2); colors[3]=0; }
            const auto indices=little32(b,cpos+4);
            std::array<unsigned,8> alpha{}; std::uint64_t alphaIndices=0;
            if (bc3) {
                alpha[0]=b[start]; alpha[1]=b[start+1];
                if(alpha[0]>alpha[1]) for(unsigned i=2;i<8;++i) alpha[i]=((8-i)*alpha[0]+(i-1)*alpha[1])/7;
                else { for(unsigned i=2;i<6;++i) alpha[i]=((6-i)*alpha[0]+(i-1)*alpha[1])/5; alpha[6]=0;alpha[7]=255; }
                for(unsigned i=0;i<6;++i) alphaIndices|=std::uint64_t(b[start+2+i])<<(i*8);
            }
            for(unsigned y=0;y<4;++y) for(unsigned x=0;x<4;++x) {
                const unsigned n=y*4+x; auto pixel=colors[(indices>>(n*2))&3];
                if(bc2) pixel=(pixel&0xffffffu)|std::uint32_t(((b[start+n/2]>>((n%2)*4))&15)*17)<<24;
                if(bc3) pixel=(pixel&0xffffffu)|std::uint32_t(alpha[(alphaIndices>>(n*3))&7])<<24;
                if(bx*4+x<width&&by*4+y<height) t->pixels[std::size_t(by*4+y)*width+bx*4+x]=pixel;
            }
        }
    } else {
        const unsigned bits=little32(b,88); const bool luminance=(flags&0x20000)!=0;
        if ((bits!=8&&bits!=16&&bits!=24&&bits!=32) || (!(flags&0x40) && !luminance))
            throw std::runtime_error("DDS uncompressed pixel format unsupported");
        const std::array<unsigned,4> masks{little32(b,92),little32(b,96),little32(b,100),little32(b,104)};
        const unsigned pixelBytes=bits/8, packedPitch=width*pixelBytes;
        const unsigned pitch=(little32(b,8)&8)?little32(b,20):packedPitch;
        if(pitch<packedPitch || 128ull+std::uint64_t(pitch)*height>b.size()) throw std::runtime_error("Truncated DDS pixels");
        auto channel=[](unsigned value,unsigned mask,unsigned absent) {
            if(!mask) return absent;
            unsigned shift=0;while((mask&1)==0){mask>>=1;++shift;}
            return unsigned((std::uint64_t((value>>shift)&mask)*255+mask/2)/mask);
        };
        for(unsigned y=0;y<height;++y) for(unsigned x=0;x<width;++x) {
            unsigned value=0; for(unsigned i=0;i<pixelBytes;++i) value|=unsigned(b[128+std::size_t(y)*pitch+x*pixelBytes+i])<<(i*8);
            const unsigned r=channel(value,masks[0],0),g=luminance?r:channel(value,masks[1],0),bl=luminance?r:channel(value,masks[2],0),a=channel(value,masks[3],255);
            t->pixels[std::size_t(y)*width+x]=(a<<24)|(r<<16)|(g<<8)|bl;
        }
    }
    t->finish(); return t;
}
std::shared_ptr<Texture> decode_image(const std::filesystem::path& path) {
    auto ext=path.extension().wstring(); std::transform(ext.begin(),ext.end(),ext.begin(),::towlower);
    if(ext==L".dds") return decode_dds(path);
    #ifdef _WIN32
    Bitmap source(path.c_str(),FALSE);
    if(source.GetLastStatus()!=Ok) throw std::runtime_error("Cannot decode image: "+path.u8string());
    auto t=std::make_shared<Texture>(); t->width=static_cast<int>(source.GetWidth()); t->height=static_cast<int>(source.GetHeight());
    if(!t->width||!t->height||t->width>16384||t->height>16384) throw std::runtime_error("Invalid image dimensions");
    t->pixels.resize(std::size_t(t->width)*t->height);
    Rect rect(0,0,t->width,t->height);BitmapData data{};
    if(source.LockBits(&rect,ImageLockModeRead,PixelFormat32bppARGB,&data)!=Ok) throw std::runtime_error("Cannot read decoded image");
    for(int y=0;y<t->height;++y) std::memcpy(t->pixels.data()+std::size_t(y)*t->width,static_cast<BYTE*>(data.Scan0)+std::ptrdiff_t(y)*data.Stride,std::size_t(t->width)*4);
    source.UnlockBits(&data); t->finish(); return t;
    #else
    auto* decoded=IMG_Load(path.c_str());
    if(!decoded)throw std::runtime_error("Cannot decode image: "+path.u8string()+": "+IMG_GetError());
    auto* surface=SDL_ConvertSurfaceFormat(decoded,SDL_PIXELFORMAT_ARGB8888,0);
    SDL_FreeSurface(decoded);
    if(!surface)throw std::runtime_error(SDL_GetError());
    std::unique_ptr<SDL_Surface,decltype(&SDL_FreeSurface)> owned(surface,SDL_FreeSurface);
    auto t=std::make_shared<Texture>();t->width=surface->w;t->height=surface->h;
    if(t->width<=0||t->height<=0||t->width>16384||t->height>16384)throw std::runtime_error("Invalid image dimensions");
    t->pixels.resize(std::size_t(t->width)*t->height);
    for(int y=0;y<t->height;++y)std::memcpy(t->pixels.data()+std::size_t(y)*t->width,static_cast<const char*>(surface->pixels)+std::ptrdiff_t(y)*surface->pitch,std::size_t(t->width)*4);
    t->finish();return t;
    #endif
}
float clamp(float v){return std::clamp(v,0.0f,1.0f);}
unsigned byte(float v){return static_cast<unsigned>(std::lround(clamp(v)*255));}
float linear_to_srgb(float v){v=clamp(v);return v<=0.0031308f?12.92f*v:1.055f*std::pow(v,1.0f/2.4f)-0.055f;}
float srgb_to_linear(float v){return v<=0.04045f?v/12.92f:std::pow((v+0.055f)/1.055f,2.4f);}
std::vector<std::uint32_t> codepoints(const std::string& text) {
    std::vector<std::uint32_t> result;
    for(std::size_t i=0;i<text.size();) {
        const auto c=static_cast<unsigned char>(text[i++]); std::uint32_t value=c; unsigned count=0;
        if(c>=0xf0){value=c&7;count=3;}else if(c>=0xe0){value=c&15;count=2;}else if(c>=0xc0){value=c&31;count=1;}
        else if(c>=0x80) throw std::runtime_error("Invalid UTF-8 text");
        for(unsigned k=0;k<count;++k){if(i==text.size()||(static_cast<unsigned char>(text[i])&0xc0)!=0x80) throw std::runtime_error("Invalid UTF-8 text");value=(value<<6)|(static_cast<unsigned char>(text[i++])&63);}
        result.push_back(value);
    }return result;
}
// Byte offsets at UTF-8 codepoint boundaries, including the one-past-end offset.
// The string natives use codepoint indices, whereas Lua's own string library
// indexes bytes. Validate complete scalar values before returning a slice.
std::vector<std::size_t> utf8_boundaries(const std::string& text) {
    std::vector<std::size_t> result{0};
    for(std::size_t i=0;i<text.size();) {
        const auto first=static_cast<unsigned char>(text[i++]);
        std::uint32_t value=first,minimum=0;unsigned remaining=0;
        if(first<=0x7f) {}
        else if(first>=0xc2&&first<=0xdf){value=first&0x1f;minimum=0x80;remaining=1;}
        else if(first>=0xe0&&first<=0xef){value=first&0x0f;minimum=0x800;remaining=2;}
        else if(first>=0xf0&&first<=0xf4){value=first&7;minimum=0x10000;remaining=3;}
        else throw std::runtime_error("Invalid UTF-8 text");
        for(unsigned j=0;j<remaining;++j){
            if(i==text.size())throw std::runtime_error("Truncated UTF-8 text");
            const auto next=static_cast<unsigned char>(text[i++]);
            if((next&0xc0)!=0x80)throw std::runtime_error("Invalid UTF-8 continuation byte");
            value=(value<<6)|(next&0x3f);
        }
        if(value<minimum||value>0x10ffff||(value>=0xd800&&value<=0xdfff))
            throw std::runtime_error("Invalid UTF-8 scalar value");
        result.push_back(i);
    }
    return result;
}
}

struct Renderer::Impl {
    struct Glyph {unsigned x=0,y=0,w=0,h=0;int dx=0,dy=0;float advance=0;};
    struct Overlay {std::shared_ptr<Texture> texture,tinted;
#ifdef _WIN32
    std::unique_ptr<Bitmap> raster;
#endif
    const Texture* rasterTexture=nullptr;int rasterWidth=0,rasterHeight=0;std::array<float,8> rasterUV{};std::array<float,8> uv{0,0,0,1,1,0,1,1};std::array<float,4> color{1,1,1,1};float angle=0,cx=0,cy=0;};
    struct Line {std::vector<std::uint32_t> chars;float width=0;std::size_t consumed=0;};
    ResourceResolver resolver;int width,height,nextOverlay=1,missingAssets=0;
#ifdef _WIN32
    ULONG_PTR token=0;
    std::unique_ptr<Bitmap> canvas;std::unique_ptr<Graphics> graphics;
#else
    SDL_Surface* canvas=nullptr;
    SDL_Renderer* graphics=nullptr;
#endif
    std::unordered_map<int,Overlay> overlays;
    std::map<std::filesystem::path,std::weak_ptr<Texture>> textureCache;
    std::unordered_map<std::uint32_t,std::array<Glyph,2>> glyphs;
    std::shared_ptr<Texture> fontTexture; float nominalFontSize=40;
    std::map<std::tuple<std::uint32_t,bool,int>,std::shared_ptr<Texture>> glyphCache;
    std::array<float,4> textColor{1,1,1,1};bool bold=false;int alignment=0,verticalAlignment=0,firstLine=0,lineLimit=0;
    float wrapWidth=0,lineScale=1;

    Impl(ResourceResolver r,int w,int h):resolver(std::move(r)),width(w),height(h) {
        if(width<=0||height<=0||width>16384||height>16384) throw std::runtime_error("Invalid renderer size");
        #ifdef _WIN32
        GdiplusStartupInput startup;
        if(GdiplusStartup(&token,&startup,nullptr)!=Ok) throw std::runtime_error("GDI+ startup failed");
        try {
            canvas=std::make_unique<Bitmap>(width,height,PixelFormat32bppARGB);
            if(canvas->GetLastStatus()!=Ok) throw std::runtime_error("Cannot allocate menu canvas");
            reset_graphics();load_font();
        }catch(...){graphics.reset();canvas.reset();GdiplusShutdown(token);token=0;throw;}
    }
    ~Impl(){graphics.reset();overlays.clear();glyphCache.clear();fontTexture.reset();canvas.reset();if(token)GdiplusShutdown(token);}
    void reset_graphics() {
        graphics=std::make_unique<Graphics>(canvas.get());
        graphics->SetCompositingMode(CompositingModeSourceOver);
        graphics->SetCompositingQuality(CompositingQualityHighQuality);
        graphics->SetInterpolationMode(InterpolationModeHighQualityBilinear);
        graphics->SetPixelOffsetMode(PixelOffsetModeHalf);
    }
    #else
        canvas=SDL_CreateRGBSurfaceWithFormat(0,width,height,32,SDL_PIXELFORMAT_ARGB8888);
        if(!canvas)throw std::runtime_error(SDL_GetError());
        graphics=SDL_CreateSoftwareRenderer(canvas);
        if(!graphics){SDL_FreeSurface(canvas);throw std::runtime_error(SDL_GetError());}
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"linear");
        try{load_font();}catch(...){SDL_DestroyRenderer(graphics);SDL_FreeSurface(canvas);throw;}
    }
    ~Impl(){overlays.clear();glyphCache.clear();fontTexture.reset();SDL_DestroyRenderer(graphics);SDL_FreeSurface(canvas);}
    // SDL owns only the platform drawing transport. DDS decoding, UV origin,
    // original font metrics, SDF coverage and GUI layout remain shared.
    void copy(Texture& texture,const SDL_Rect& src,const SDL_FRect& dst,double angle=0,const SDL_FPoint* center=nullptr,SDL_RendererFlip flip=SDL_FLIP_NONE,const std::array<float,4>* color=nullptr){
        if(!texture.bitmap){
            texture.bitmap=SDL_CreateTexture(graphics,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STATIC,texture.width,texture.height);
            if(!texture.bitmap)throw std::runtime_error(SDL_GetError());
            if(SDL_UpdateTexture(texture.bitmap,nullptr,texture.pixels.data(),texture.width*4)!=0||SDL_SetTextureBlendMode(texture.bitmap,SDL_BLENDMODE_BLEND)!=0)throw std::runtime_error(SDL_GetError());
        }
        SDL_SetTextureColorMod(texture.bitmap,color?byte(linear_to_srgb((*color)[0])):255,color?byte(linear_to_srgb((*color)[1])):255,color?byte(linear_to_srgb((*color)[2])):255);
        SDL_SetTextureAlphaMod(texture.bitmap,color?byte((*color)[3]):255);
        if(SDL_RenderCopyExF(graphics,texture.bitmap,&src,&dst,angle,center,flip)!=0)throw std::runtime_error(SDL_GetError());
    }
    #endif
    std::shared_ptr<Texture> load_texture(const std::string& name) {
        try {
            const auto path=resolver(name);
            if(auto existing=textureCache[path].lock())return existing;
            auto t=decode_image(path);textureCache[path]=t;return t;
        }catch(...){++missingAssets;throw;}
    }
    void load_font() {
        const Bytes b=read_bytes(resolver("shared/font/defaultFont.font"));
        nominalFontSize=big_float(b,0);const auto count=big32(b,8);
        if(!std::isfinite(nominalFontSize)||nominalFontSize<=0||count>65536||b.size()!=12ull+count*28ull)throw std::runtime_error("Unsupported bitmap font metrics");
        fontTexture=load_texture("shared/font/defaultFont.png");
        for(unsigned i=0;i<count;++i){const std::size_t p=12+std::size_t(i)*28;std::array<Glyph,2> pair;
            for(unsigned j=0;j<2;++j){const std::size_t k=p+4+j*12;auto& g=pair[j];g.x=unsigned(b[k])<<8|b[k+1];g.y=unsigned(b[k+2])<<8|b[k+3];g.w=b[k+4];g.h=b[k+5];g.dx=static_cast<std::int8_t>(b[k+6]);g.dy=static_cast<std::int8_t>(b[k+7]);g.advance=big_float(b,k+8);
                if(g.x+g.w>unsigned(fontTexture->width)||g.y+g.h>unsigned(fontTexture->height)||!std::isfinite(g.advance)||g.advance<0)throw std::runtime_error("Invalid bitmap font glyph");}
            glyphs.emplace(big32(b,p),pair);
        }
    }
    const Glyph& glyph(std::uint32_t c)const {
        auto found=glyphs.find(c);if(found==glyphs.end())found=glyphs.find('?');
        if(found==glyphs.end())throw std::runtime_error("Font missing fallback glyph");
        return found->second[bold?1:0];
    }
    std::vector<Line> layout(float pixelSize,const std::string& text)const {
        auto chars=codepoints(text);std::vector<Line> lines(1);const float scale=pixelSize/nominalFontSize,maxWidth=wrapWidth*width;
        for(std::size_t i=0;i<chars.size();++i){const auto c=chars[i];if(c=='\r')continue;
            if(c=='\n'){lines.back().consumed=i+1;lines.emplace_back();continue;}
            float advance=glyph(c).advance*scale;
            if(maxWidth>0 && !lines.back().chars.empty() && lines.back().width+advance>maxWidth){
                auto& old=lines.back();auto split=std::find(old.chars.rbegin(),old.chars.rend(),std::uint32_t(' '));
                if(split!=old.chars.rend()) {
                    const auto index=old.chars.size()-1-std::size_t(split-old.chars.rbegin());
                    Line next;next.chars.assign(old.chars.begin()+index+1,old.chars.end());old.chars.resize(index);
                    old.width=0;for(auto v:old.chars)old.width+=glyph(v).advance*scale;
                    next.width=0;for(auto v:next.chars)next.width+=glyph(v).advance*scale;old.consumed=i-next.chars.size();lines.push_back(std::move(next));
                } else {old.consumed=i;lines.emplace_back();}
                if(c==' '){lines.back().consumed=i+1;continue;}
            }
            lines.back().chars.push_back(c);lines.back().width+=advance;lines.back().consumed=i+1;
        }return lines;
    }
    std::shared_ptr<Texture> raster_glyph(std::uint32_t c,float pixelSize) {
        const int keySize=std::max(1,int(std::lround(pixelSize*64)));const auto key=std::make_tuple(c,bold,keySize);
        auto it=glyphCache.find(key);if(it!=glyphCache.end())return it->second;
        const auto& g=glyph(c);const float scale=float(keySize)/64/nominalFontSize;
        auto t=std::make_shared<Texture>();t->width=std::max(1,int(std::ceil(g.w*scale)));t->height=std::max(1,int(std::ceil(g.h*scale)));t->pixels.resize(std::size_t(t->width)*t->height);
        // The original PNG stores a signed-distance field in its luminance.
        // Reconstruct coverage after sampling at the output resolution.
        const float edge=std::max(3.0f,7.5f/std::max(0.01f,scale));
        auto sample=[&](int x,int y){x=std::clamp(x,int(g.x),int(g.x+g.w)-1);y=std::clamp(y,int(g.y),int(g.y+g.h)-1);return float(fontTexture->pixels[std::size_t(y)*fontTexture->width+x]&255);};
        for(int y=0;y<t->height;++y)for(int x=0;x<t->width;++x){
            const float sx=g.x+(x+0.5f)/scale-0.5f,sy=g.y+(y+0.5f)/scale-0.5f;const int ix=int(std::floor(sx)),iy=int(std::floor(sy));const float fx=sx-ix,fy=sy-iy;
            const float value=(sample(ix,iy)*(1-fx)+sample(ix+1,iy)*fx)*(1-fy)+(sample(ix,iy+1)*(1-fx)+sample(ix+1,iy+1)*fx)*fy;
            float a=clamp((value-(128-edge))/(2*edge));a=a*a*(3-2*a);t->pixels[std::size_t(y)*t->width+x]=(byte(a)<<24)|0xffffffu;
        }
        t->finish();glyphCache.emplace(key,t);return t;
    }
    Overlay& overlay(lua_State* L){int id=int(luaL_checkinteger(L,1));auto it=overlays.find(id);if(it==overlays.end())throw std::runtime_error("Unknown overlay id: "+std::to_string(id));return it->second;}
    static Impl& self(lua_State* L){return *static_cast<Impl*>(lua_touserdata(L,lua_upvalueindex(1)));}
    template<int(Impl::*Method)(lua_State*)>static int invoke(lua_State* L){try{return(self(L).*Method)(L);}catch(const std::exception& e){return luaL_error(L,"%s",e.what());}catch(...){return luaL_error(L,"Renderer failure");}}
    int create_image(lua_State* L){const std::string name=luaL_checkstring(L,1);Overlay o;o.texture=load_texture(name);const int id=nextOverlay++;overlays.emplace(id,std::move(o));lua_pushinteger(L,id);return 1;}
    int set_uv(lua_State* L){auto& o=overlay(L);for(int i=0;i<8;++i)o.uv[i]=float(luaL_checknumber(L,i+2));return 0;}
    int set_color(lua_State* L){auto& o=overlay(L);std::array<float,4> value;for(int i=0;i<4;++i)value[i]=clamp(float(luaL_checknumber(L,i+2)));if(value!=o.color){o.color=value;o.tinted.reset();
#ifdef _WIN32
        o.raster.reset();
#endif
        }return 0;}
    int set_rotation(lua_State* L){auto& o=overlay(L);o.angle=float(luaL_checknumber(L,2));o.cx=float(luaL_optnumber(L,3,0));o.cy=float(luaL_optnumber(L,4,0));return 0;}
    int draw_overlay(lua_State* L){auto& o=overlay(L);const float x=float(luaL_checknumber(L,2)),y=float(luaL_checknumber(L,3)),w=float(luaL_checknumber(L,4)),h=float(luaL_checknumber(L,5));
        if(w==0||h==0||o.color[3]<=0)return 0;
        if(std::abs(o.uv[0]-o.uv[2])>0.00001f||std::abs(o.uv[4]-o.uv[6])>0.00001f||std::abs(o.uv[1]-o.uv[5])>0.00001f||std::abs(o.uv[3]-o.uv[7])>0.00001f)throw std::runtime_error("Nonrectangular overlay UVs are unsupported");
        auto texture=o.texture;
        if(o.color!=std::array<float,4>{1,1,1,1}){
            if(!o.tinted){o.tinted=std::make_shared<Texture>();o.tinted->width=texture->width;o.tinted->height=texture->height;o.tinted->pixels.resize(texture->pixels.size());
                std::array<std::array<unsigned,256>,3> tables{};for(unsigned c=0;c<3;++c)for(unsigned i=0;i<256;++i)tables[c][i]=byte(linear_to_srgb(srgb_to_linear(float(i)/255)*o.color[c]));
                for(std::size_t i=0;i<texture->pixels.size();++i){auto p=texture->pixels[i];o.tinted->pixels[i]=(byte(float(p>>24)/255*o.color[3])<<24)|(tables[0][(p>>16)&255]<<16)|(tables[1][(p>>8)&255]<<8)|tables[2][p&255];}o.tinted->finish();}
            texture=o.tinted;
        }
        // DDS/native overlay UVs use the lower left as their texture origin.
        const float left=o.uv[0]*texture->width,right=o.uv[4]*texture->width,top=(1-o.uv[3])*texture->height,bottom=(1-o.uv[1])*texture->height;
        if(left==right||top==bottom)return 0;
        #ifdef _WIN32
        const auto saved=graphics->Save();
        const float centerX=(x+o.cx)*width,centerY=(1-y-o.cy)*height;
        if(o.angle!=0){graphics->TranslateTransform(centerX,centerY);graphics->RotateTransform(-o.angle*180/3.14159265358979323846f);graphics->TranslateTransform(-centerX,-centerY);}
        PointF dest[3]{{x*width,(1-y-h)*height},{(x+w)*width,(1-y-h)*height},{x*width,(1-y)*height}};
        if(right<left)std::swap(dest[0].X,dest[1].X),dest[2].X=dest[0].X;
        if(bottom<top)std::swap(dest[0].Y,dest[2].Y),dest[1].Y=dest[0].Y;
        ImageAttributes attributes;attributes.SetWrapMode(WrapModeClamp);
        const int rasterWidth=int(std::lround(w*width)),rasterHeight=int(std::lround(h*height)),rasterX=int(std::lround(x*width)),rasterY=int(std::lround((1-y-h)*height));
        const bool useCache=texture->opaque&&o.angle==0&&right>left&&bottom>top&&rasterWidth>0&&rasterHeight>0&&rasterWidth<=4096&&rasterHeight<=4096&&std::int64_t(rasterWidth)*rasterHeight>=100000
            &&std::abs(w*width-rasterWidth)<0.001f&&std::abs(h*height-rasterHeight)<0.001f&&std::abs(x*width-rasterX)<0.001f&&std::abs((1-y-h)*height-rasterY)<0.001f;
        Status status;
        if(useCache){
            // Scaling the static 2048-square main background each frame is
            // expensive with GDI+. Cache only opaque, integer-aligned draws;
            // alpha overlays and fractional geometry retain the normal path.
            if(!o.raster||o.rasterTexture!=texture.get()||o.rasterWidth!=rasterWidth||o.rasterHeight!=rasterHeight||o.rasterUV!=o.uv){
                o.raster=std::make_unique<Bitmap>(rasterWidth,rasterHeight,PixelFormat32bppARGB);Graphics cache(o.raster.get());
                cache.SetCompositingMode(CompositingModeSourceCopy);cache.SetCompositingQuality(CompositingQualityHighQuality);cache.SetInterpolationMode(InterpolationModeHighQualityBilinear);cache.SetPixelOffsetMode(PixelOffsetModeHalf);
                PointF corners[3]{{0,0},{float(rasterWidth),0},{0,float(rasterHeight)}};
                status=cache.DrawImage(texture->bitmap.get(),corners,3,left,top,right-left,bottom-top,UnitPixel,&attributes);cache.Flush(FlushIntentionSync);
                if(status!=Ok){o.raster.reset();graphics->Restore(saved);throw std::runtime_error("Background cache drawing failed");}
                o.rasterTexture=texture.get();o.rasterWidth=rasterWidth;o.rasterHeight=rasterHeight;o.rasterUV=o.uv;
            }
            status=graphics->DrawImage(o.raster.get(),rasterX,rasterY,rasterWidth,rasterHeight);
        }else status=graphics->DrawImage(texture->bitmap.get(),dest,3,std::min(left,right),std::min(top,bottom),std::abs(right-left),std::abs(bottom-top),UnitPixel,&attributes);
        graphics->Restore(saved);if(status!=Ok)throw std::runtime_error("Overlay drawing failed");return 0;
        #else
        const SDL_Rect src{int(std::floor(std::min(left,right))),int(std::floor(std::min(top,bottom))),
            std::max(1,int(std::ceil(std::abs(right-left)))),std::max(1,int(std::ceil(std::abs(bottom-top))))};
        const SDL_FRect dst{x*width,(1-y-h)*height,w*width,h*height};
        if(w<0||h<0)throw std::runtime_error("Negative overlay dimensions are unsupported");
        const SDL_FPoint center{o.cx*width,(h-o.cy)*height};
        const auto flip=static_cast<SDL_RendererFlip>((right<left?SDL_FLIP_HORIZONTAL:0)|(bottom<top?SDL_FLIP_VERTICAL:0));
        copy(*texture,src,dst,-o.angle*180/3.14159265358979323846,&center,flip);
        return 0;
        #endif
    }

    int set_text_color(lua_State* L){for(int i=0;i<4;++i)textColor[i]=clamp(float(luaL_checknumber(L,i+1)));return 0;}
    int set_bold(lua_State* L){bold=lua_toboolean(L,1)!=0;return 0;}
    int set_alignment(lua_State* L){alignment=int(luaL_checkinteger(L,1));return 0;}
    int set_vertical(lua_State* L){verticalAlignment=int(luaL_checkinteger(L,1));return 0;}
    int set_wrap(lua_State* L){wrapWidth=std::max(0.0f,float(luaL_checknumber(L,1)));return 0;}
    int set_line_scale(lua_State* L){lineScale=std::max(0.01f,float(luaL_checknumber(L,1)));return 0;}
    int set_line_bounds(lua_State* L){firstLine=std::max(0,int(luaL_checkinteger(L,1)));lineLimit=std::max(0,int(luaL_checkinteger(L,2)));return 0;}
    int text_width(lua_State* L){const float size=std::max(0.0f,float(luaL_checknumber(L,1)))*height;auto lines=layout(size,luaL_checkstring(L,2));float maxWidth=0;for(auto&line:lines)maxWidth=std::max(maxWidth,line.width);lua_pushnumber(L,maxWidth/width);return 1;}
    int text_height(lua_State* L){const float size=std::max(0.0f,float(luaL_checknumber(L,1)));auto lines=layout(size*height,luaL_checkstring(L,2));lua_pushnumber(L,size*lineScale*lines.size());lua_pushinteger(L,lines.size());return 2;}
    int text_line_height(lua_State* L){lua_pushnumber(L,luaL_checknumber(L,1)*lineScale);return 1;}
    int text_length(lua_State* L){const float size=std::max(0.0f,float(luaL_checknumber(L,1)))*height;auto lines=layout(size,luaL_checkstring(L,2));const int limit=int(luaL_checkinteger(L,3));std::size_t length=0;for(int i=0;i<limit&&i<int(lines.size());++i)length=lines[i].consumed;lua_pushinteger(L,length);return 1;}
    int utf8_length(lua_State* L){
        std::size_t bytes=0;const auto* text=luaL_checklstring(L,1,&bytes);
        lua_pushinteger(L,static_cast<lua_Integer>(utf8_boundaries(std::string(text,bytes)).size()-1));return 1;
    }
    int utf8_substring(lua_State* L){
        std::size_t bytes=0;const auto* data=luaL_checklstring(L,1,&bytes);
        const auto boundaries=utf8_boundaries(std::string(data,bytes));
        const auto count=boundaries.size()-1;
        const lua_Integer start=luaL_checkinteger(L,2);
        const lua_Integer requested=luaL_optinteger(L,3,static_cast<lua_Integer>(count));
        if(start<0||requested<0)throw std::runtime_error("UTF-8 substring start and length must be nonnegative");
        const auto first=std::min(static_cast<std::size_t>(start),count);
        const auto length=std::min(static_cast<std::size_t>(requested),count-first);
        lua_pushlstring(L,data+boundaries[first],boundaries[first+length]-boundaries[first]);return 1;
    }
    int text_line_length(lua_State* L){
        const float size=std::max(0.0f,float(luaL_checknumber(L,1)))*height;
        std::size_t bytes=0;const auto* data=luaL_checklstring(L,2,&bytes);const std::string text(data,bytes);
        utf8_boundaries(text);
        const float maximum=float(luaL_checknumber(L,3))*width;
        if(!std::isfinite(size)||!std::isfinite(maximum))throw std::runtime_error("Text measurement requires finite size and width");
        const auto chars=codepoints(text);float used=0;std::size_t count=0;
        if(maximum>=0)for(auto c:chars){
            if(c=='\n')break;
            const float advance=c=='\r'?0:glyph(c).advance*size/nominalFontSize;
            if(used+advance>maximum)break;
            used+=advance;++count;
        }
        lua_pushinteger(L,static_cast<lua_Integer>(count));return 1;
    }
    int draw_text(lua_State* L){const float x=float(luaL_checknumber(L,1))*width,y=(1-float(luaL_checknumber(L,2)))*height,size=std::max(0.0f,float(luaL_checknumber(L,3)))*height;const auto lines=layout(size,luaL_checkstring(L,4));if(size<=0||textColor[3]<=0)return 0;
        const int end=lineLimit?std::min(int(lines.size()),firstLine+lineLimit):int(lines.size());float baseline=y;
        if(verticalAlignment==1)baseline+=size*0.5f;else if(verticalAlignment==2)baseline+=size;
        #ifdef _WIN32
        ColorMatrix color{};color.m[0][0]=linear_to_srgb(textColor[0]);color.m[1][1]=linear_to_srgb(textColor[1]);color.m[2][2]=linear_to_srgb(textColor[2]);color.m[3][3]=textColor[3];color.m[4][4]=1;
        ImageAttributes attributes;attributes.SetColorMatrix(&color);attributes.SetWrapMode(WrapModeClamp);
        // SDF coverage is already sampled and antialiased at the output size.
        // GDI+'s high-quality resampling adds rectangular halos to these small
        // transparent glyph bitmaps. Copy their coverage without a second filter.
        const auto previousInterpolation=graphics->GetInterpolationMode();
        graphics->SetInterpolationMode(InterpolationModeNearestNeighbor);
        #endif
        for(int i=firstLine;i<end;++i){const auto&line=lines[i];float cursor=x;if(alignment==1)cursor-=line.width/2;else if(alignment==2)cursor-=line.width;
            const float lineY=baseline-(firstLine-i)*size*lineScale;
            for(auto c:line.chars){const auto& g=glyph(c);const float scale=size/nominalFontSize;
                if(c!=' '&&g.w&&g.h){auto raster=raster_glyph(c,size);
                    #ifdef _WIN32
                    RectF dest(cursor+g.dx*scale,lineY-g.dy*scale,g.w*scale,g.h*scale);graphics->DrawImage(raster->bitmap.get(),dest,0,0,float(raster->width),float(raster->height),UnitPixel,&attributes);
                    #else
                    const SDL_Rect src{0,0,raster->width,raster->height};
                    const SDL_FRect dst{cursor+g.dx*scale,lineY-g.dy*scale,g.w*scale,g.h*scale};
                    copy(*raster,src,dst,0,nullptr,SDL_FLIP_NONE,&textColor);
                    #endif
                }
                cursor+=g.advance*scale;
            }
        }
        #ifdef _WIN32
        graphics->SetInterpolationMode(previousInterpolation);
        #endif
        return 0;
    }
#ifdef _WIN32
    int set_clip(lua_State* L){if(lua_gettop(L)==0||lua_isnil(L,1)){graphics->ResetClip();return 0;}float x=float(luaL_checknumber(L,1)),y=float(luaL_checknumber(L,2)),w=float(luaL_checknumber(L,3)),h=float(luaL_checknumber(L,4));graphics->SetClip(RectF(x*width,(1-y-h)*height,w*width,h*height),CombineModeReplace);return 0;}
    int reset_clip(lua_State*){graphics->ResetClip();return 0;}
#else
    int reset_clip(lua_State*){if(SDL_RenderSetClipRect(graphics,nullptr)!=0)throw std::runtime_error(SDL_GetError());return 0;}
    int set_clip(lua_State* L){
        if(lua_gettop(L)==0||lua_isnil(L,1))return reset_clip(L);
        const float x=float(luaL_checknumber(L,1)),y=float(luaL_checknumber(L,2)),w=float(luaL_checknumber(L,3)),h=float(luaL_checknumber(L,4));
        const SDL_Rect clip{int(std::floor(x*width)),int(std::floor((1-y-h)*height)),std::max(0,int(std::ceil(w*width))),std::max(0,int(std::ceil(h*height)))};
        if(SDL_RenderSetClipRect(graphics,&clip)!=0)throw std::runtime_error(SDL_GetError());
        return 0;
    }
#endif
    void bind(lua_State* L,const char* name,lua_CFunction fn){lua_pushlightuserdata(L,this);lua_pushcclosure(L,fn,1);lua_setglobal(L,name);}
    void register_lua(lua_State* L){
#define BIND(Name,Method) bind(L,Name,&invoke<&Impl::Method>)
        BIND("createImageOverlay",create_image);BIND("setOverlayUVs",set_uv);BIND("setOverlayColor",set_color);BIND("setOverlayRotation",set_rotation);BIND("renderOverlay",draw_overlay);
        BIND("setTextColor",set_text_color);BIND("setTextBold",set_bold);BIND("setTextAlignment",set_alignment);BIND("setTextVerticalAlignment",set_vertical);BIND("setTextWrapWidth",set_wrap);BIND("setTextLineHeightScale",set_line_scale);BIND("setTextLineBounds",set_line_bounds);
        BIND("getTextWidth",text_width);BIND("getTextHeight",text_height);BIND("getTextLineHeight",text_line_height);BIND("getTextLength",text_length);BIND("renderText",draw_text);BIND("set2DClipArea",set_clip);BIND("reset2DClipArea",reset_clip);
        BIND("utf8Strlen",utf8_length);BIND("utf8Substr",utf8_substring);BIND("getTextLineLength",text_line_length);
#undef BIND
        lua_newtable(L);for(auto value:{std::pair<const char*,int>{"ALIGN_LEFT",0},{"ALIGN_CENTER",1},{"ALIGN_RIGHT",2}}){lua_pushinteger(L,value.second);lua_setfield(L,-2,value.first);}lua_setglobal(L,"RenderText");
        // Check the public Lua bridge itself, including multi-byte characters,
        // embedded NULs and actual normal/bold font advances. Restore text state.
        const bool savedBold=bold;
        const int check=luaL_dostring(L,R"lua(
            local text=string.char(65,0xc3,0xa9,0xe4,0xb8,0xad,0xf0,0x9f,0x9a,0x9c,90)
            assert(utf8Strlen(text)==5 and utf8Strlen('')==0)
            assert(utf8Substr(text,1,3)==text:sub(2,-2))
            assert(utf8Substr(text,4)=='Z' and utf8Substr(text,5)=='')
            assert(utf8Substr(text,0,0)=='' and utf8Substr(text,99)=='')
            assert(utf8Strlen('A'..string.char(0)..'B')==3)
            assert(utf8Substr('A'..string.char(0)..'B',1,1)==string.char(0))
            for _,bad in ipairs({string.char(0xc0,0x80),string.char(0xed,0xa0,0x80),string.char(0xf4,0x90,0x80,0x80),string.char(0xe4,0xb8)}) do
                assert(not pcall(utf8Strlen,bad))
            end
            for _,isBold in ipairs({false,true}) do
                setTextBold(isBold)
                local size=0.03125
                local prefix=getTextWidth(size,'AB')
                assert(getTextLineLength(size,'ABC',prefix+0.000001)==2)
                assert(getTextLineLength(size,'ABC',prefix-0.000001)==1)
                assert(getTextLineLength(size,text,getTextWidth(size,text)+0.000001)==5)
                assert(getTextLineLength(size,'A',-1)==0)
                assert(getTextLineLength(size,'',1)==0)
            end
        )lua");
        bold=savedBold;
        if(check!=0){const std::string message=lua_tostring(L,-1);lua_pop(L,1);throw std::runtime_error("Text bridge self-check failed: "+message);}
    }
};

Renderer::Renderer(ResourceResolver r,int width,int height):impl_(std::make_unique<Impl>(std::move(r),width,height)){}
Renderer::~Renderer()=default;
#ifdef _WIN32
void Renderer::begin_frame(){impl_->graphics->ResetTransform();impl_->graphics->ResetClip();impl_->graphics->Clear(Color(255,0,0,0));}
void Renderer::present(HDC dc){impl_->graphics->Flush(FlushIntentionSync);Graphics target(dc);target.DrawImage(impl_->canvas.get(),0,0,impl_->width,impl_->height);}
void Renderer::save_png(const std::filesystem::path& path){
    impl_->graphics->Flush(FlushIntentionSync);impl_->graphics.reset();
    UINT count=0,size=0;GetImageEncodersSize(&count,&size);std::vector<BYTE> storage(size);auto encoders=reinterpret_cast<ImageCodecInfo*>(storage.data());GetImageEncoders(count,size,encoders);CLSID encoder{};bool found=false;
    for(UINT i=0;i<count;++i)if(std::wcscmp(encoders[i].MimeType,L"image/png")==0){encoder=encoders[i].Clsid;found=true;break;}
    auto status=found?impl_->canvas->Save(path.c_str(),&encoder,nullptr):UnknownImageFormat;impl_->reset_graphics();
    if(status!=Ok)throw std::runtime_error("Cannot save menu screenshot: "+path.u8string());
}
#else
void Renderer::begin_frame(){
    SDL_RenderSetClipRect(impl_->graphics,nullptr);SDL_SetRenderDrawColor(impl_->graphics,0,0,0,255);
    if(SDL_RenderClear(impl_->graphics)!=0)throw std::runtime_error(SDL_GetError());
}
void Renderer::present(DrawTarget window){
    SDL_RenderPresent(impl_->graphics);
    auto* target=SDL_GetWindowSurface(window);
    if(!target||SDL_BlitScaled(impl_->canvas,nullptr,target,nullptr)!=0||SDL_UpdateWindowSurface(window)!=0)throw std::runtime_error(SDL_GetError());
}
void Renderer::save_png(const std::filesystem::path& path){
    SDL_RenderPresent(impl_->graphics);
    if(IMG_SavePNG(impl_->canvas,path.c_str())!=0)throw std::runtime_error("Cannot save screenshot: "+std::string(IMG_GetError()));
}
#endif
bool Renderer::delete_overlay(int id){return impl_->overlays.erase(id)!=0;}
int Renderer::overlay_count()const{return static_cast<int>(impl_->overlays.size());}
int Renderer::missing_asset_count()const{return impl_->missingAssets;}
void Renderer::register_lua(lua_State* L){impl_->register_lua(L);}
}
