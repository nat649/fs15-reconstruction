#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>

namespace fs15world {
struct Vertex { float x,y,z,nx,ny,nz,u,v; };
struct Node {
    std::string name, type;
    int id=0, parent=-1;
    float translation[3]{0,0,0};
};
struct Terrain {
    std::string map_id;
    std::filesystem::path scene_file, height_file, texture_file;
    std::vector<Node> nodes;
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    unsigned source_width=0,source_height=0,file_references=0,deferred_shapes=0;
    float min_height=0,max_height=0,width=0,depth=0,height_scale=0,units_per_pixel=0;
};
struct Texture {
    unsigned width=0,height=0,row_pitch=0;
    int compression=0; // 1=BC1, 2=BC2, 3=BC3; retain supplied mip levels.
    std::vector<unsigned> mip_offsets,mip_pitches;
    std::vector<std::uint8_t> bytes;
};
Terrain load_terrain(const std::filesystem::path& resource_root,const std::string& map_id);
Texture load_dds(const std::filesystem::path& filename);
void save_png(const std::filesystem::path& filename,unsigned width,unsigned height,
              const std::uint8_t* bgra,unsigned row_pitch);
}
