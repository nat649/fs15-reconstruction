# Original GE6 startup on Linux x64

The active task is to run the original GIANTS Engine 6 on Linux. ReFarm and the
separate original-object preview are fixtures, not this runtime or its startup.

`FS15GE6LinuxBoot` is an experimental Linux ELF launcher for the user's private,
version-pinned original x64 instruction image. It preserves native GE6 code and
data, maps PE sections at their original addresses and binds supported OS
imports to Linux services through the Microsoft x64 calling convention. Neither
Wine nor Proton is linked or invoked. No replacement renderer, physics world,
menu or game engine is linked to this executable.

The initial entry is the inspected original CRT at `0x1403fc96c`, followed by
`0x1403fc7f4` and its original initialization chain. The platform bootstrap
section `.bind` is not executed. The normalized private analysis image is the
same reference used by the Windows original-code checks, SHA-256
`6d1beaf9d7e3c57dd51cec03385a53bda82b4ac946e622d8d2fa592a56d71f9f`.
Original instructions are not distributed.

## Build and execute

On Linux x86_64:

```sh
sudo apt-get install g++ cmake libssl-dev
cmake -S src/ge6_linux -B build/ge6-linux-boot -DCMAKE_BUILD_TYPE=Release
cmake --build build/ge6-linux-boot --parallel 2
build/ge6-linux-boot/FS15GE6LinuxBoot \
  --image inputs/runtime/FarmingSimulator2015Game.runtime.bin
```

The root portable CMake also exposes `FS15_BUILD_GE6_LINUX_BOOT=ON`. Linux
workflow artifacts include `bin/FS15GE6LinuxBoot`; this remains a diagnostic,
not a playable GE6 build. OpenSSL only checks the original-image SHA-256.

OS bindings use Linux clocks/process/thread identifiers, allocations, recursive
mutexes and bounded main-thread CRT slot/module services. These are authored
platform transport, not recovered engine code. `GetVersion` supplies API
compatibility level 6, not the host OS version. Complete Windows DLL loading,
SEH/unwind, TEB/PEB, extra threads, filesystem, window/OpenGL, audio, networking
and Steam services remain unimplemented here.

Each unimplemented import has a named stop trampoline. Calling one prints
`GE6_BLOCKED_IMPORT library!symbol caller=...` and exits with status 78; it never
returns fabricated engine/render initialization success. Native instruction
faults report their instruction address separately.

## Validation boundaries

`tests/check_ge6_linux_boot.py` uses authored PE instructions to check mapping,
writable zero-filled data, an actual Microsoft-ABI import call/writeback, a
named missing-import stop, hash rejection and overlapping-section rejection.
Those fixtures do not boot original GE6 code.

`scripts/check_ge6_linux_vm.py` runs the private original image in a local QEMU
Linux VM using an existing Alpine kernel/initramfs and a static launcher build
(`FS15_GE6_BOOT_STATIC=ON`). No network device is attached. Its generated
`private-ge6-initramfs.gz` embeds original code and must stay local. The serial
log distinguishes CRT execution, missing services, native faults and exits.

PE mapping follows the [Microsoft specification](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format).
The ABI bridge uses GCC's [`ms_abi` attribute](https://gcc.gnu.org/onlinedocs/gcc/x86-Function-Attributes.html).

Acceptance milestones are original CRT completion, original application startup,
renderer/window creation, resource loading and a native GE6 menu/map session.
Mapping or passing an authored ABI fixture does not establish these milestones.
