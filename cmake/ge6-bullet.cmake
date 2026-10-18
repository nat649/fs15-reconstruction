# Upstream middleware dependency, not recovered GIANTS source. GE6 contains
# the old userPointer/userIndex union, the ignore-collision array and the newer
# row-solver function pointers. Released 2.82 and 2.83 do not both match these.
include(FetchContent)
if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()
FetchContent_Declare(GE6Bullet
  URL https://github.com/bulletphysics/bullet3/archive/0e1a77047cfaa9bf94f7318686ba4179f40c8c1b.tar.gz
  URL_HASH SHA256=46f19770d23a3b05fb40e181214b977a1451b04324bdb3ba081b82149c41684f
  SOURCE_SUBDIR _fs15_manual_library_selection)
# Use only middleware libraries; upstream's old demo/toolchain configuration
# would change the parent's flags and introduce unrelated dependencies.
FetchContent_MakeAvailable(GE6Bullet)
set(BULLET_PHYSICS_SOURCE_DIR "${ge6bullet_SOURCE_DIR}")
set(BULLET_VERSION 2.82)
set(INSTALL_LIBS OFF)
foreach(component LinearMath BulletCollision BulletDynamics)
  add_subdirectory("${ge6bullet_SOURCE_DIR}/src/${component}" "${ge6bullet_BINARY_DIR}/${component}" EXCLUDE_FROM_ALL)
  set_target_properties(${component} PROPERTIES POSITION_INDEPENDENT_CODE ON)
  if(NOT MSVC)
    target_compile_options(${component} PRIVATE -ffp-contract=off)
  endif()
endforeach()
install(FILES "${CMAKE_SOURCE_DIR}/licenses/Bullet-LICENSE.txt" DESTINATION share/fs15-reconstruction/licenses)
