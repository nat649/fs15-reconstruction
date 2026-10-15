#include "../src/engine/collision.h"
#include "../src/engine/density.h"
#include <DirectXMath.h>
#include <fstream>
#include <stdexcept>
#include <limits>
#include <cstring>
#include <iostream>
static void require(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
int main(int argc,char** argv){try{
    if(argc!=2)throw std::runtime_error("Expected a fixture directory");std::filesystem::path dir=argv[1];std::filesystem::create_directories(dir);
    fs15world::Terrain scene;fs15world::Mesh mesh;
    for(auto p:std::vector<DirectX::XMFLOAT3>{{0,0,-5},{0,4,-5},{0,4,5},{0,0,5}})mesh.vertices.push_back({p.x,p.y,p.z,1,0,0,0,0});
    mesh.indices={0,1,2,0,2,3};scene.meshes.push_back(mesh);fs15world::Node node;node.collidable=true;node.mesh=0;DirectX::XMFLOAT4X4 identity;DirectX::XMStoreFloat4x4(&identity,DirectX::XMMatrixIdentity());std::copy(&identity._11,&identity._11+16,node.world);scene.nodes.push_back(node);
    CollisionWorld wall(scene);require(wall.triangle_count()==2,"Wall collision mesh was lost");require(wall.blocked(0.5f,0,0,0.8f),"Wall penetration not detected");require(!wall.blocked(1.5f,0,0,0.8f),"Unrelated point blocked");require(!wall.blocked(0.5f,0,5,0.8f),"Wall height ignored");require(wall.blocked(-0.5f,-4,0,0.8f),"Negative grid cells ignored");
    scene.nodes[0].world[12]=32;CollisionWorld moved(scene);require(!moved.blocked(0.5f,0,0,0.8f)&&moved.blocked(32.5f,0,0,0.8f),"Original collider transform not used");
    scene.nodes[0].collidable=false;CollisionWorld trigger(scene);require(!trigger.blocked(32.5f,0,0,0.8f),"Trigger treated as solid wall");
    auto make=[&](unsigned word_bytes,const std::vector<unsigned char>& payload,unsigned w=2,unsigned h=2){std::vector<unsigned char> bytes(21);std::memcpy(bytes.data(),"GRLE\1",5);auto put=[&](unsigned off,unsigned v){std::memcpy(bytes.data()+off,&v,4);};put(5,w);put(9,h);put(13,word_bytes);put(17,unsigned(payload.size()));bytes.insert(bytes.end(),payload.begin(),payload.end());auto file=dir/"world-service-fixture.grle";std::ofstream out(file,std::ios::binary);out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());out.close();return file;};
    auto grid=fs15world::load_grle(make(2,{1,1,1,1,2,0}));require(grid.values==std::vector<std::uint16_t>(4,257),"GRLE16 extended values changed");
    grid=fs15world::load_grle(make(1,{7,7,2}));require(grid.values==std::vector<std::uint16_t>(4,7),"GRLE8 run changed");
    auto rejects=[&](const std::filesystem::path& path){try{fs15world::load_grle(path);}catch(const std::exception&){return;}throw std::runtime_error("Malformed GRLE accepted");};
    rejects(make(2,{1,0,1,0,255,255}));rejects(make(2,{1,0}));rejects(make(1,{7,7,255}));rejects(make(3,{7,7,2}));rejects(make(1,{7,7,2},5000,2));
    std::cout<<"Static wall transforms/heights/triggers and bounded GRLE fixtures passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
