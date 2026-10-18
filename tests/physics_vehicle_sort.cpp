#include "physics_vehicle_sort.h"
#include <array>
#include <iostream>
int main(){
    std::array<uintptr_t,8> tokens{0,1,2,3,4,5,6,7};
    const std::array<uintptr_t,8> equal_order{5,4,7,6,1,0,3,2};
    if(!fs15_physics_vehicle_sort(tokens.data(),tokens.size(),[](void*,uintptr_t){return 0;},nullptr)||tokens!=equal_order)return 1;
    std::array<int,8> keys{7,-2,3,0,7,-8,1,2};
    if(!fs15_physics_vehicle_sort(tokens.data(),tokens.size(),[](void* p,uintptr_t token){return static_cast<int*>(p)[token];},keys.data()))return 1;
    for(size_t i=1;i<tokens.size();++i)if(keys[tokens[i]]<keys[tokens[i-1]])return 1;
    if(!fs15_physics_vehicle_sort(nullptr,0,nullptr,nullptr)||fs15_physics_vehicle_sort(nullptr,2,nullptr,nullptr)||fs15_physics_vehicle_sort(tokens.data(),1000001,nullptr,nullptr))return 1;
    std::cout<<"GE6 vehicle island sort: duplicate-key swap order, signed keys and bounds passed\n";
}
