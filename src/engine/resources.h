#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>
#include <map>

namespace fs15world {
struct Vertex { float x,y,z,nx,ny,nz,u,v; };
struct Node {
    std::string name, type;
    int id=0, parent=-1;
    float translation[3]{0,0,0};
    float rotation[3]{0,0,0}, scale[3]{1,1,1};
    float world[16]{};
    std::vector<int> children;
    std::map<std::string,std::string> attributes;
    bool visible=true;
    bool collidable=false;
    int mesh=-1;
};
struct Matrix { float value[16]; };
struct Subset { unsigned first=0,count=0; };
struct Mesh { unsigned id=0; std::vector<Vertex> vertices; std::vector<std::uint32_t> indices; std::vector<Subset> subsets; };
struct Material { std::filesystem::path texture,original_shader; float color[4]{1,1,1,1}; bool alpha=false; bool water=false; unsigned effect=0; float effect_params[4]{}; };
struct MeshGroup { unsigned mesh=0; std::vector<int> materials; std::vector<Matrix> instances; bool casts_shadow=false,receives_shadow=true; std::vector<int> nodes; };
struct Layer { std::filesystem::path detail,weight; float unit_size=1; };
struct WeightImage { unsigned width=0,height=0; std::vector<std::uint8_t> pixels; };
struct Sun { bool shadows=false; unsigned resolution=512; float distance=80,bias=0.0012f,slope=0.0005f,extrusion=200; };
struct FoliageLayer {std::string name;unsigned type=0,offset=0,channels=0,states=1;int material=0;float blocks=1;std::vector<unsigned> shapes;std::vector<float> widths,heights;};
struct Terrain {
    std::string map_id;
    std::filesystem::path scene_file, height_file, texture_file;
    std::vector<Node> nodes;
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    unsigned source_width=0,source_height=0,file_references=0,shape_nodes=0;
    float min_height=0,max_height=0,width=0,depth=0,height_scale=0,units_per_pixel=0;
    std::vector<Mesh> meshes;
    std::map<int,Material> materials;
    std::vector<MeshGroup> groups;
    unsigned rendered_instances=0,skipped_instances=0;
    std::uint64_t object_triangles=0;
    std::vector<Layer> layers;
    std::vector<std::filesystem::path> used_resources;
    unsigned road_alignment_samples=0;
    float road_median_error=0,road_flipped_z_error=0;
    Sun sun;
    std::vector<std::uint16_t> heights;
    std::vector<FoliageLayer> foliage;
};
struct Texture {
    unsigned width=0,height=0,row_pitch=0;
    int compression=0; // 1=BC1, 2=BC2, 3=BC3; retain supplied mip levels.
    std::vector<unsigned> mip_offsets,mip_pitches;
    std::vector<std::uint8_t> bytes;
};
Terrain load_terrain(const std::filesystem::path& resource_root,const std::string& map_id);
Terrain load_scene(const std::filesystem::path& resource_root,const std::filesystem::path& relative);
void update_transforms(Terrain& scene,const Matrix& root);
float terrain_height(const Terrain& terrain,float x,float z);
Texture load_dds(const std::filesystem::path& filename);
std::vector<Mesh> load_meshes(const std::filesystem::path& root,const std::filesystem::path& relative);
std::filesystem::path resolve_resource(const std::filesystem::path& root,const std::filesystem::path& relative);
std::string shader_injection(const std::filesystem::path& root,const std::filesystem::path& relative,const std::string& position);
WeightImage load_weight(const std::filesystem::path& filename);
void save_png(const std::filesystem::path& filename,unsigned width,unsigned height,
              const std::uint8_t* bgra,unsigned row_pitch);
}
