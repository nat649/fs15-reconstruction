#include "physics_vehicle_sort.h"
#include <utility>
namespace {
void partition(uintptr_t* data,int first,int last,int (*key)(void*,uintptr_t),void* user){
    do{
        int left=first,right=last;const uintptr_t pivot=data[(first+last)/2];
        do{
            int pk=key(user,pivot);int current=key(user,data[left]);
            while(current<pk){++left;pk=key(user,pivot);current=key(user,data[left]);}
            current=key(user,data[right]);pk=key(user,pivot);
            while(pk<current){--right;current=key(user,data[right]);pk=key(user,pivot);}
            if(right<left)break;
            std::swap(data[left],data[right]);++left;--right;
        }while(left<=right);
        if(first<right)partition(data,first,right,key,user);
        if(last<=left)return;
        first=left;
    }while(true);
}
}
extern "C" int fs15_physics_vehicle_sort(uintptr_t* data,size_t count,int (*key)(void*,uintptr_t),void* user){
    if(count>1000000||(!data&&count)||(!key&&count>1))return 0;
    if(count>1)partition(data,0,static_cast<int>(count)-1,key,user);
    return 1;
}
