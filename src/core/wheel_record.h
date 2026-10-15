#pragma once
#include <cstddef>
#include <cstdint>
#include <limits>

namespace fs15 {
// Layout recovered from native lookup 0x1400c9e50 and seven query adapters.
// Unknown bytes remain explicit. These types are not a recovered PhysX solver.
struct WheelRecord {
    std::uint8_t unknown00[0x30];
    float axle_speed;
    std::uint8_t contact_active, unknown35[3];
    std::uint32_t contact_object;
    std::uint8_t unknown3c[4];
    float contact_point[3];
    std::uint8_t unknown4c[4];
    float contact_point_aux;
    std::uint8_t unknown54[12];
    float contact_normal[3];
    std::uint8_t unknown6c[4];
    float contact_force;
    std::uint8_t unknown74[12];
    float position[3];
    std::uint8_t unknown8c[4];
    float drive_angle, slip[2];
    std::uint8_t unknown9c[0x40];
    float radius;
    std::uint8_t unknowne0[0x30];
    float lateral_stiffness, reference_load, longitudinal_stiffness, friction;
    std::uint8_t unknown120[0x1c];
    std::int32_t shape_id;
    std::uint8_t unknown140[0x20];
};
struct WheelSet {
    std::uint8_t unknown00[0x3c];
    std::int32_t count;
    std::uint8_t unknown40[8];
    WheelRecord* records;
};
struct WheelTreeNode {
    WheelTreeNode *left, *parent, *right;
    std::uint64_t actor_key;
    WheelSet* value;
    std::uint8_t color, is_nil, unknown2a[6];
};
struct PhysicsManagerView { std::uint8_t unknown00[0x250]; WheelTreeNode* wheels; };
struct PhysicsActorData {
    std::uint8_t unknown00[8];
    std::uint32_t flags;
    std::uint8_t unknown0c[0x24];
    std::uint64_t actor_key;
};
struct SceneObjectView { std::uint8_t unknown00[0x118]; PhysicsActorData* physics; };
static_assert(sizeof(void*)==8,"Recovered object pointer layouts currently target 64-bit builds");
static_assert(sizeof(float)==4 && std::numeric_limits<float>::is_iec559,"Recovered fields need IEEE binary32");
static_assert(sizeof(WheelRecord)==0x160 && offsetof(WheelRecord,shape_id)==0x13c);
static_assert(offsetof(WheelRecord,contact_active)==0x34 && offsetof(WheelRecord,contact_object)==0x38);
static_assert(offsetof(WheelRecord,contact_point)==0x40 && offsetof(WheelRecord,contact_point_aux)==0x50);
static_assert(offsetof(WheelRecord,contact_normal)==0x60 && offsetof(WheelRecord,contact_force)==0x70);
static_assert(offsetof(WheelRecord,position)==0x80 && offsetof(WheelRecord,drive_angle)==0x90);
static_assert(offsetof(WheelRecord,slip)==0x94 && offsetof(WheelRecord,radius)==0xdc);
static_assert(offsetof(WheelRecord,lateral_stiffness)==0x110 && offsetof(WheelRecord,friction)==0x11c);
static_assert(offsetof(WheelSet,count)==0x3c && offsetof(WheelSet,records)==0x48);
static_assert(offsetof(WheelTreeNode,is_nil)==0x29 && sizeof(WheelTreeNode)==0x30);
static_assert(offsetof(PhysicsManagerView,wheels)==0x250 && offsetof(SceneObjectView,physics)==0x118);
static_assert(offsetof(PhysicsActorData,actor_key)==0x30);
}
