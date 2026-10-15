# Intermediate CPU/ABI portability check. This is NOT a Nintendo/libnx toolchain.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
find_program(FS15_QEMU_AARCH64 qemu-aarch64 REQUIRED)
set(CMAKE_CROSSCOMPILING_EMULATOR "${FS15_QEMU_AARCH64};-L;/usr/aarch64-linux-gnu")
