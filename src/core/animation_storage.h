#pragma once
// Internal storage shared by recovered binary/XML resource construction.
#include "animation_resources.h"
namespace fs15_animation_storage {
FS15AnimationChannel* channel(const FS15SceneLifecycleServices&);
FS15AnimationClip* clip(const char*,size_t,float,const FS15SceneLifecycleServices&);
void destroy_channel(FS15AnimationChannel*,const FS15SceneLifecycleServices&);
void destroy_clip(FS15AnimationClip*,const FS15SceneLifecycleServices&);
}
