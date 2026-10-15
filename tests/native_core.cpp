#include "api.h"
#include "wheel_record.h"
#include <array>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>

static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){
    try {
        std::array<std::uint8_t,8> output{};
        const std::uint8_t stream8[]={7,7,3,9};
        require(fs15_decode_rle8(stream8,4,8,8,output.data(),output.size())==6,"RLE8 decoded count");
        require(output[4]==7&&output[5]==9,"RLE8 literal/run order");
        const std::uint16_t stream16[]={0x1234,0x1234,1,0xabcd};
        require(fs15_decode_rle16(stream16,4,16,16,output.data(),output.size())==4,"RLE16 decoded count");
        require(output[0]==0x34&&output[1]==0x12&&output[6]==0xcd&&output[7]==0xab,"RLE16 little-endian wire layout");
        require(fs15_decode_rle8(stream8,4,8,8,output.data(),1)==std::numeric_limits<std::size_t>::max(),"RLE capacity guard");
        const float parameters[]={0.5f,2,1,8,1.1f};float forces[4]={1,1,1,1};
        require(fs15_compute_tire_forces(parameters,0,0,1,forces)==1,"Tire force call");
        for(float value:forces)require(value==0,"Stationary tire force");
        require(fs15_compute_tire_forces(nullptr,0,0,1,forces)==0,"Tire input guard");
        std::array<std::uint8_t,0x100> bit_frame{};
        std::uint32_t bit_a=0x80000001,bit_b=33,bit_result=0;
        std::memcpy(bit_frame.data(),&bit_a,4);std::memcpy(bit_frame.data()+16,&bit_b,4);
        require(fs15_script_bit_operation(FS15_BIT_SHIFT_LEFT,bit_frame.data(),bit_frame.size())==1,"Shift call");
        std::memcpy(&bit_result,bit_frame.data()+0xf0,4);require(bit_result==2,"Masked shift and wrapping");
        require(fs15_script_bit_operation(FS15_BIT_HIGHEST_SET,bit_frame.data(),bit_frame.size())==1,"Highest-bit call");
        std::memcpy(&bit_result,bit_frame.data()+0xf0,4);require(bit_result==31,"Highest-bit unsigned input");
        require(fs15_script_bit_operation(99,bit_frame.data(),bit_frame.size())==0,"Unknown bit operation guard");
        fs15::WheelRecord records[3]{};records[0].shape_id=7;records[1].shape_id=-2;records[2].shape_id=7;
        records[0].axle_speed=-12.5f;records[0].contact_active=1;records[0].contact_force=900;
        records[0].contact_point[0]=1.5f;records[0].contact_point[1]=2.5f;records[0].contact_point[2]=3.5f;
        records[0].contact_point_aux=4.5f;records[0].slip[0]=-0.25f;records[0].slip[1]=0.75f;
        fs15::WheelSet set{};set.count=3;set.records=records;
        fs15::WheelTreeNode sentinel{},node{};sentinel.is_nil=1;sentinel.parent=&node;
        node.left=node.right=&sentinel;node.actor_key=0xfffffffffffffff0ULL;node.value=&set;
        fs15::PhysicsManagerView manager{};manager.wheels=&sentinel;
        fs15::PhysicsActorData physics{};physics.flags=8;physics.actor_key=node.actor_key;
        fs15::SceneObjectView object{};object.physics=&physics;
        const void* found=nullptr;
        require(fs15_resolve_wheel_record(&manager,&object,7,&found)==&records[0]&&found==&set,"First duplicate wheel selection");
        require(fs15_resolve_wheel_record(&manager,&object,-2,&found)==&records[1],"Signed wheel identifier");
        require(fs15_resolve_wheel_record(&manager,&object,99,&found)==nullptr&&found==&set,"Missing wheel preserves found vehicle");
        physics.flags=0;require(fs15_resolve_wheel_record(&manager,&object,7,&found)==nullptr,"Actor flag filter");physics.flags=8;
        physics.actor_key=0;require(fs15_resolve_wheel_record(&manager,&object,7,&found)==nullptr,"Unsigned tree key lower bound");physics.actor_key=node.actor_key;
        node.left=&node;require(fs15_resolve_wheel_record(&manager,&object,7,&found)==nullptr,"Malformed tree cycle bound");node.left=&sentinel;
        std::array<std::uint8_t,0x130> frame;frame.fill(0xa5);auto before=frame;
        require(fs15_query_wheel_record(FS15_WHEEL_CONTACT_POINT,&records[0],sizeof(records[0]),1,frame.data(),frame.size())==1,"Contact point query");
        for(unsigned i=0;i<4;++i){float value=0;std::uint16_t type=0;std::memcpy(&value,frame.data()+0xf0+i*16,4);std::memcpy(&type,frame.data()+0xf8+i*16,2);require(value==1.5f+i&&type==3,"Contact point value/tag");}
        records[0].contact_active=0;frame=before;
        require(fs15_query_wheel_record(FS15_WHEEL_CONTACT_POINT,&records[0],sizeof(records[0]),1,frame.data(),frame.size())==1,"No-contact query");
        for(unsigned i=0;i<4;++i){std::uint16_t type=9;std::memcpy(&type,frame.data()+0xf8+i*16,2);require(type==0&&frame[0xf0+i*16]==0xa5,"Nil tags leave payload unchanged");}
        frame=before;require(fs15_query_wheel_record(FS15_WHEEL_SLIP,&records[0],sizeof(records[0]),0,frame.data(),frame.size())==1&&frame==before,"Null scene object frame preservation");
        require(fs15_query_wheel_record(FS15_WHEEL_POSITION,nullptr,0,1,frame.data(),frame.size())==0&&frame==before,"Undefined original position guarded");
        require(fs15_query_wheel_record(99,&records[0],sizeof(records[0]),1,frame.data(),frame.size())==0&&frame==before,"Unknown query guard");
        require(fs15_query_wheel_record(FS15_WHEEL_SLIP,&records[0],4,1,frame.data(),frame.size())==0&&frame==before,"Truncated record guard");
        std::cout<<"Portable recovered components passed: RLE8/RLE16, tire math, wheel lookup and script-slot queries. No GE6 startup or console build tested.\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
