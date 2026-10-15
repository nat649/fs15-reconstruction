#pragma once
#include "resources.h"
#include <DirectXMath.h>
#include <map>
#include <vector>
// Temporary static capsule/wall service over original collision triangles.
// No PhysX joints, suspension, rollovers or dynamic rigid bodies are emulated.
class CollisionWorld {
    struct Triangle {DirectX::XMFLOAT3 a,b,c;};
    std::vector<Triangle> triangles;
    std::map<std::pair<int,int>,std::vector<unsigned>> cells;
public:
    explicit CollisionWorld(const fs15world::Terrain&);
    bool blocked(float x,float z,float base,float radius)const;
    unsigned triangle_count()const{return unsigned(triangles.size());}
};
