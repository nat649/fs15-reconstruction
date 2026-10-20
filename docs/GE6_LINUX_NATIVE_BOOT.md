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
The loader also recognizes the separately pinned, already prepared **Local GE6**
image described below. Original instructions are not distributed.

## Build and execute

On Linux x86_64:

```sh
sudo apt-get install g++ cmake libssl-dev libasound2-dev libpulse-dev libpng-dev libx11-dev libxrandr-dev libgl-dev
cmake -S src/ge6_linux -B build/ge6-linux-boot -DCMAKE_BUILD_TYPE=Release
cmake --build build/ge6-linux-boot --parallel 2
build/ge6-linux-boot/FS15GE6LinuxBoot \
  --image inputs/runtime/FarmingSimulator2015Game.runtime.bin
```

The root portable CMake also exposes `FS15_BUILD_GE6_LINUX_BOOT=ON`. Linux
workflow artifacts include `bin/FS15GE6LinuxBoot`; this remains a diagnostic,
not a playable GE6 build. OpenSSL checks the original-image SHA-256. CMake
downloads a SHA-256-pinned OpenAL Soft source release for Linux audio; it does
not download original game files. See [dependency and relinking materials](../src/ge6_linux/AUDIO_DEPENDENCY.md).

The normal dynamic build enables `FS15_GE6_X11=ON`. It needs a real X11 server
and host OpenGL/GLX drivers. `FS15_GE6_BOOT_STATIC=ON` retains the separate
headless diagnostic with X11 disabled. Neither build includes original files.

## First original main window and GL context (2026-10-20)

The private Local GE6 image now creates and **shows its original 1280 x 720
main window** under Linux. X11 confirms that the actual window is viewable;
original SDL supplies its window procedure at `0x14037cf00`. Original context
creation subsequently creates and activates the main window's native GLX
context. The server reports **OpenGL 4.5 Compatibility Profile, Mesa llvmpipe**.
This is distinct from the two hidden 10 x 10 probe windows/contexts, which the
original engine creates and destroys first.

The surface is currently **black**. Startup stops at
`GDI32.dll!GetDeviceGammaRamp`, caller `0x140382f02`. No original game frame,
menu, map, playable physics or working gameplay input is established by this
milestone. The next work is gamma queries and the remaining renderer imports.

The [source-pinned X11 evidence](../analysis/ge6-linux-x11-validation.json)
records the executed ELF/private log hashes, original image/resource hashes,
main-window ownership of the GL DC, original callers, actual map state and
next stop. The [raw X11 capture](screenshots/ge6-linux-native-window-2026-10-20.png)
is the blank startup surface, captured at that stop. It contains no menu or
rendered game content. Original code/resources, private VM and SSH keys stay
outside Git.

This run uses a private local QEMU VM with Linux `6.18.52-0-virt`, Ubuntu 24.04
libraries and Xvfb at 1280 x 720. Xvfb is a real X server with Mesa software
OpenGL; physical GPU drivers and a tester's interactive desktop remain untested.
Networking is restricted to a host-loopback SSH forward. No Wine, Proton,
ReFarm renderer or replacement engine is linked or run. The Local GE6 input
retains the documented pre-existing Windows bootstrap modification below;
the Linux loader adds no native instruction patches.

The authored OS/ABI code maps display queries to RandR and GDI query
resources to actual X11 GCs/pixmaps. Windows styles/properties and callbacks
are bounded host metadata; native GE6 instructions still control startup.
Window geometry currently uses X11 client dimensions with decorations left to
the host window manager. Keyboard setup covers the tested default US mapping;
complete keyboard layout/input, nonclient behavior, file drops and WGL extension
parity remain open. The host double-click default is 500 ms unless its X resource
overrides it. No unavailable display mode, refresh rate or GPU identity is
invented. OpenGL exports use typed Microsoft-ABI adapters, rather than exposing
System V pointers to original Windows calls.

Dedicated CI builds both diagnostic variants. Its authored-only Xvfb fixture
checks real window creation/show/destruction, class ownership, stack arguments
and callbacks, display-mode rejection, GL float arguments and pixel readback.
These fixture pixels are separate from the blank original-engine surface.
Every executable artifact includes OpenAL source/object/relink materials and
dependency notices. Public CI never runs original game code.

The validated artifact is from [CI run 39494670722](https://github.com/nat649/fs15-reconstruction/actions/runs/39494670722),
source `ea4796c9294c8618485974d18ac9e2b1157df34a`. Both X11 and static jobs
passed, including relinking and the existing loader/ABI checks. The repository
also passes 91 Python tests (two optional checks skipped). The exact downloaded
X11 ELF was executed privately and its SHA-256 verified inside the VM.
The same ELF preserves the reference image's original Steam restart boundary
at `0x14000c8b4` with 129 OS calls.

Final validation used a fresh isolated VM profile, with its previous test
profile retained separately. Restaging newer default configuration files over
an existing profile can enter the original settings-update confirmation path;
its `ShowCursor`/`MessageBoxW` adapters remain incomplete. No confirmation
response is synthesized.

Private graphics test helpers are `scripts/setup_ge6_graphics_vm.py` and
`scripts/ge6_graphics_vm.py`. They assemble ignored local Ubuntu package inputs
with SHA-256 checks, stage only the named private startup resources, and support
compile/deploy/run/capture. `scripts/verify_ge6_x11_boot.py` checks captured
observations and emits the bounded report; it rejects probe-only contexts,
hidden windows, unrelated DCs and faulted/wrong-provenance runs.

## Observed original-code Linux result (2026-10-19)

The local QEMU x64 VM runs Linux `6.18.52-0-virt` and the private original image.
It completes the CRT initialization and global constructors, then executes the
original application entry `0x14000c9a0`. Its first timer call returns to
`0x14000c9f5`; the subsequent native singleton check and debugger query run too.
No original instruction is patched. The process then stops explicitly at
`steam_api64.dll!SteamAPI_RestartAppIfNecessary`, caller `0x14000c8b4`.

```text
GE6_ORIGINAL_MAIN_REACHED caller=0x14000c9f5
GE6_OS CreateMutexA
GE6_OS IsDebuggerPresent
GE6_BLOCKED_IMPORT steam_api64.dll!SteamAPI_RestartAppIfNecessary caller=0x14000c8b4 os_calls=126
GE6_LINUX_VM_RESULT=78
```

The [source-pinned aggregate evidence](../analysis/ge6-linux-native-boot-validation.json)
records the private log/ELF hashes and the exact source revision. Original code,
private initramfs and full serial logs remain local. This establishes original
CRT/application execution inside Linux; it does not establish core engine,
renderer/window, menu or map initialization. No Steam session or ownership is
synthesized. Its unavailable platform services remain an explicit stop.

[Dedicated Linux CI run 39233789342](https://github.com/nat649/fs15-reconstruction/actions/runs/39233789342)
passes authored loader/ABI fixtures for the same source, including six/eight
argument calls with stack arguments, CP1252 accented/euro conversion, GS stack
metadata, callback registration, and cross-process singleton presence/lifetime.
Public CI does not contain or run the original image.

## Existing Local GE6 input on Linux (2026-10-20)

The existing private `build/ge6-local/x64/FarmingSimulator2015Game.exe` has SHA-256
`e8a9f3b4397fb9aca31fdcb7471728f4f6ef7940a85911f1f727dbe09377a955`.
It is the same image previously tested on Windows, generated by
`scripts/prepare_local_engine.py`. Its original Windows Steam-only startup
function has an existing one-byte `RET` modification at `0x14000c810`; its import
name selects the authored `fs15_local.dll` contract. The Linux loader does not
add or apply instruction patches. This input must not be described as an
unmodified-instruction baseline or a completely rebuilt engine.

The Linux adapter ports the existing local-platform contract: initialization
and unavailable account/apps/statistics endpoints return false/null; the utility
interface supplies the FS15 application identifier. No identity, ownership,
entitlement or network session is synthesized. The reference image's original
`steam_api64.dll` imports remain unbound.

The actual Linux VM run now executes original profile creation, `game.xml`
copy/timestamp checks, disk read and the original configuration XML parser,
then enters the original application constructor at `0x1400010d0`. Its optional
COM initialization receives an explicit failure (`E_NOTIMPL`); the original
caller continues. Winsock startup now succeeds through POSIX sockets, followed
by original central initialization at `0x14008abe0`. GE6 writes its own journal
with its runtime banner and actual QEMU CPU/Linux memory observations. The
original script system creates its events and reserves/commits its Lua memory.
Two original workers now execute on real Linux pthreads: the Lua timer worker
at `0x140424bb0` and the engine worker wrapper at `0x140043620`. Each has its own
stack NT_TIB prefix and TLS/FLS values. A host entry marker alone is not counted
as execution: the evidence also requires a first OS call issued while executing
that worker (`timeBeginPeriod` and an event wait in the observed run).

With the unchanged original integrity manifest, checked map files and two GAR
archives staged locally, GE6 advances through its integrity-check stage and
writes its original Physics System and Input System initialization messages.
The original audio initialization now opens a real OpenAL Soft device and
context through the Linux ABI transport. Its own journal reports OpenAL 1.1,
device `OpenAL Soft` and 512 sources. This isolated VM explicitly selects the
silent `null` backend; audible desktop/game audio is not verified. The next
observed dependency after audio was `d3d9.dll!Direct3DCreate9`, caller
`0x140049a26`, in the original adapter probe. Its documented unavailable result
now lets original startup continue. The original SDL/CRT signal initialization
registers its native control handler at `0x1404009d8`. The
original SDL application-class preparation routine at `0x14037deb0` now
continues: the named optional icon is absent (resource error 1814), and the class
record retains original procedure `0x14037cf00` with atom 49152. No substitute
icon or OS window is fabricated. Dynamic USER32 lookup succeeds for the authored
API view; the three optional touch exports remain unavailable, and original SDL
formats those errors and continues. The earlier headless checkpoint's stop was
`USER32.dll!EnumDisplayDevicesW`, caller `0x140380ae4`, in actual display
enumeration. Neither a window nor a graphics context has been created. The
pinned run binds 153 imported names and records 1,667 OS/audio calls before
stopping; scheduling may change the number of worker timer calls. Import totals are not completion
percentages. These startup log sections do not
demonstrate a simulated game world, delivered input, a rendered window or
playable physics.

The original file manager, configuration parser, logger and Lua allocation code
remain in control; these additions are authored OS transport. `Z:/` maps
explicitly to the Linux root because the original
Windows engine recognizes drive-qualified absolute paths, not POSIX `/` paths.
Other drives and drive-relative paths are rejected.

The [separate local-image evidence](../analysis/ge6-linux-local-boot-validation.json)
records the input/source/ELF/log hashes and the current stop; it does not replace
the reference-image evidence above. Dedicated
[Linux CI run 39467694877](https://github.com/nat649/fs15-reconstruction/actions/runs/39467694877)
passes authored ABI, Unicode, file lifecycle/search/copy/timestamp, drive mapping,
compatibility-version and unavailable-platform contracts for that source,
plus real UDP loopback, timer-slack/affinity, CPU/memory/registry queries,
manual/auto-reset event wakeups, virtual-memory protection/lifetime checks and
actual pthread callback/TLS/FLS lifecycle tests, plus OpenAL PCM mixing and
Microsoft float ABI checks and actual console-signal callbacks. An authored PE fixture also
creates and waits for PE worker instructions through a six-argument Microsoft
ABI import, checks the child's GS stack/self metadata and verifies isolated TLS.
Resource/class tests additionally check real PNG/DIB pixels, copied class
metadata and retained icon ownership. Dedicated PE fixtures verify six-argument
image loading/WNDCLASSW layout and seven-argument UTF-16 error-message output.
Unicode/ANSI module-reference tests verify implemented exports, unavailable
exports, invalid UTF-16 rejection and released references. Class registration
is recorded separately from OS-window/render/menu initialization.
Public CI does not contain the original game. The missing optional
`preloadMemory.pak` produces an ordinary file-not-found result and the original
engine continues to configuration loading; no cache contents are synthesized.
The private serial log also captures the original journal after exit. Only its
captured-block hash, bounded startup observations and resource hashes are published. Its
`Windows NT 6.0` text comes from original logging of the compatibility API level;
the actual host kernel remains Linux. Journal output is evidence of partial
initialization, not proof of a menu, renderer or loaded game world.

The same audio-enabled Linux executable was also retested against the
unmodified-instruction reference image: it still stops at the original
`SteamAPI_RestartAppIfNecessary`, caller `0x14000c8b4` (129 traced OS calls).
The earlier source-pinned reference report is retained separately; the audio
and unavailable graphics bindings do not synthesize platform access.

The private VM invocation can stage startup configuration files from the local
prepared game directory:

```sh
python3 scripts/check_ge6_linux_vm.py \
  --qemu /path/to/qemu-system-x86_64 \
  --kernel /path/to/vmlinuz-virt --alpine-initramfs /path/to/initramfs-virt \
  --exe build/ge6-linux-boot/FS15GE6LinuxBoot \
  --image build/ge6-local/x64/FarmingSimulator2015Game.exe \
  --resources build/ge6-local --integrity-resources --archives \
  --output build/private-ge6-linux-vm
```

The helper passes the original `-profile Z:/profile` option and explicitly sets
`ALSOFT_DRIVERS=null`, recorded separately in the evidence. It leaves the
original audio-enabled game configuration unchanged. By default it stages
only four named startup files; `--archives` additionally stages the two original
GAR archives locally. `--integrity-resources` adds the original `crc_files.xml`
and its two checked files, `data/maps/map01.i3d` and `data/maps/map01/map01_dem.png`.
Their contents/checksums are not rewritten and original integrity code is not
disabled. The helper records staged-file hashes and preserves partial private
serial output on timeout. It attaches no network device. Generated image archives
and full logs embed or describe private input and remain under ignored `build/`.

## OS transport and remaining work

The loader privately copies the validated readable PE resource directory into
bounded host storage. `LoadImageW` resolves group-icon names or integer IDs,
then decodes the first original frame for the observed zero-size/default-color
request. PNG frames use Linux libpng/zlib; uncompressed 8/24/32-bit DIB frames
use an authored palette/BGRA/orientation/AND-mask adapter. This is Windows image
resource transport, not recovered engine image-loading code. Authored fixtures
verify pixels, alpha/masks, IDs/names, shared/nonshared lifetime, missing-resource
errors and malformed directory/data bounds. A separate PE fixture loads an
authored icon through the six-argument IAT call and supplies a native WNDCLASSW.

`RegisterClassW` stores a copied ASCII class name, the original Microsoft-ABI
procedure, style/extra-byte counts and a retained icon reference. It returns an
atom for that actual process-local record and rejects duplicate registrations.
Unregistering releases the record and checks window references. This is class
metadata for subsequent window creation; it does not create an OS window or
call the procedure. Menus, cursors, brushes, global/DLL class namespaces and
full Unicode class-name comparison remain open. Unsupported image types,
file/system images, resizing/other LoadImage flags and unsupported class
parameters stop explicitly; missing named resources return their actual error.
The [Windows image API](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-loadimagew)
and [class registration API](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerclassw)
define the transported boundary.

`LoadLibraryW` transcodes validated UTF-16 into the same bounded authored-module
lookup/reference tracking used by `LoadLibraryA`. Null, malformed and missing
names retain failure/error behavior. This does not load arbitrary Windows DLL
instructions or provide a full Windows search path/dependency loader. Optional
unavailable modules remain unavailable. The authored USER32 module view exposes
the same implemented Microsoft-ABI image/class functions through GetProcAddress;
unimplemented exports return unavailable, never a System V pointer.
SDL's optional export probes format their failures with `FormatMessageW` before
continuing. Its observed FROM_SYSTEM call now supplies bounded English text for
module/procedure-not-found errors (126/127), preserving UTF-16 counts, caller
buffers and LastError on success. A seven-argument authored PE fixture checks
the output and its untouched guard. This is not a complete localized Windows
message catalogue: unsupported standard message IDs/formatting flags stop;
absent high IDs and unsupported languages return failure.

All 32 statically imported OpenAL functions have typed Microsoft-x64 wrappers
which call the actual Linux OpenAL Soft 1.24.3 implementation. Device/context
creation, errors, buffers, source queues and mixing are provided by that
third-party library. Only the two Windows router default selectors map to the
Linux default device; explicit device names are preserved and unknown ones
fail. No System V extension function pointer is returned to original PE code.
ALSA and PulseAudio backends are compiled in; the VM/CI select silent output.
Authored tests verify actual PCM loopback energy, queue order, source state,
errors and cleanup. A separate authored PE fixture calls the wrappers through
its IAT and checks the Windows XMM1 argument for a listener gain value. The
workflow also relinks the supplied object/archive and repeats the mixer test.
Complete upstream source, notices and relinking materials accompany installed
boot artifacts. This reuses Linux audio middleware; it does not reconstruct
GE6's engine logic or the original Windows OpenAL DLL.

There is no Direct3D9 service in this Linux transport. Its factory reports
documented failure (`nullptr`), allowing the original optional adapter probe
to choose its own continuation. It does not create an adapter, synthesize a GPU
identity or implement Direct3D rendering. The original application supports a
separate default graphics path; connecting its window/context APIs remains work.

`SetConsoleCtrlHandler` registers/removes actual callbacks, following reverse
registration order and stopping at a handled event. Linux `SIGINT` maps to
`CTRL_C_EVENT`, and `SIGQUIT` to `CTRL_BREAK_EVENT`; the null-handler ignore flag
affects only Ctrl+C. The POSIX signal routine writes to a nonblocking pipe;
it never calls original PE code or takes C++ locks. A dispatcher starts a fresh
thread for each queued event, installs the GS stack prefix, calls the Microsoft
ABI handlers and performs FLS cleanup before restoring GS. Notifications are
serialized. Host shutdown joins this dispatcher before unmapping original code.
The two pipe descriptors remain owned until process exit so an in-flight signal
handler cannot write into a recycled descriptor. Initialization failures after
signal installation also retain those descriptors and restore host dispositions.
See the [Windows API contract](https://learn.microsoft.com/en-us/windows/console/setconsolectrlhandler).

Authored fixtures send real SIGINT/SIGQUIT, check order, ignore/removal (including
self-removal), distinct callback TIDs, GS/TLS isolation and FLS cleanup. A small
authored PE receives a real SIGINT through the IAT-registered callback and checks
its GS metadata and distinct TID. An unhandled signal test exits with Linux
status 130. Close/logoff/shutdown console events, console attachment/reset,
debugger exceptions, child ignore-flag inheritance and complete Windows process
status semantics remain unsupported; pipe saturation/POSIX signals can coalesce
notifications. The original-game boot test observes **registration only**; it
does not inject a control event or establish original handler return/shutdown.
Aggregate evidence distinguishes registration from a completed callback.

OS bindings use Linux clocks/process/thread identifiers, allocations, recursive
mutexes and bounded main-thread CRT slot/module services. These are authored
platform transport, not recovered engine code. `GetVersion` supplies API
compatibility level 6, not the host OS version. A small GS-based NT_TIB prefix
provides actual Linux thread stack bounds for original MSVC stack probes;
Linux libc's FS-based TLS is unchanged. CP1252 classification/case tables are
numeric Windows OS observations generated by `capture_ge6_ansi_reference.py`;
only the captured characters/locales/flags are supported. Environment staging
is still ASCII-bounded. Valid UTF-8/UTF-16 strings support non-ASCII names and
surrogate pairs; malformed input is rejected, so Windows flags-zero replacement
behavior is not complete. Synchronous file creation/read/write/seek/size/search,
copy and timestamp queries use POSIX services. Copies preserve permissions and
access/modification timestamps; unavailable creation timestamps are zero rather
than a fabricated POSIX change time. Windows sharing, case-insensitivity,
overlapped I/O and full NTFS metadata semantics are not reproduced.
`GetVersion`/`GetVersionExA` expose compatibility API level 6.0, not a Windows
host; the VM reports its actual Linux kernel separately.
`CoInitializeEx` explicitly fails: no COM apartment, object activation or
DirectX/COM service is supplied. Its return value is checked in authored tests;
the observed original startup caller tolerates the failure.
The bounded network adapter negotiates Winsock versions in the x64 `WSADATA`
layout, probes actual IPv4 UDP support, tracks startup/cleanup references and
owns real POSIX sockets. Its direct UDP bind/name/send/receive calls translate
errors and report truncation. TCP, DNS, IPv6, overlapped socket APIs and full
Winsock provider behavior remain unimplemented. This does not validate FS15
multiplayer or an online session. The isolated original-code VM has no network
device; UDP loopback is tested separately on the Linux CI host.

Main-thread timer requests check Linux clock resolution, bound timer slack and
restore it when leases end. Affinity uses actual Linux CPU masks within the
supported low 64 CPUs; it rejects unsupported handles/masks. Full Windows
multimedia/process-wide scheduling behavior is not reproduced. Unnamed events
support auto/manual reset and finite/infinite waits through mutexes/condition
variables. Non-alertable `WaitForSingleObjectEx` calls use that same transport;
alertable/APC waits stop explicitly. Named events remain unavailable.

`CreateThread` uses a Linux pthread with the PE's default stack reserve or a
bounded requested size. Every worker installs its own inspected NT_TIB prefix
without changing Linux FS TLS, then invokes the original callback with the
Microsoft ABI. Initially suspended creation, `ResumeThread`, wait/status queries
and handle closure are connected. Closing a handle does not cancel the worker;
normal callback return performs FLS callbacks before restoring GS and signaling
completion. Separate TLS (1088) and FLS (128) index pools hold per-thread values,
clear values on index reuse and dispatch non-null FLS callbacks on free/exit.
This supplies the observed native CRT storage mechanism; fibers and general
Windows TLS/CRT/DLL thread lifecycle parity are not claimed.

Shared critical-section, file/socket/key/module handle registries are protected
against concurrent mutation. Process exit and missing-import stops terminate
directly instead of destructing/unmapping code beneath live native workers.
Authored fixtures verify two workers, isolated errors/slots, 1000 protected
counter updates, real stack bounds and FLS callbacks. The original VM workers
are alive when the startup dependency stop ends the process; complete original worker return,
shutdown and cleanup remain unverified. `ExitThread`, arbitrary thread suspension,
termination, priorities, exception dispatch and processor groups remain open.

Only the documented legacy debugger naming packet (`0x406d1388`, validated
24-byte structure) is consumed by `RaiseException`, and maps to actual Linux
thread names truncated to 15 bytes. Other exception codes/payloads stop; C++
exceptions, assertions and engine failures are not ignored. Environment queries
use the actual Linux process and remain ASCII-bounded/case-sensitive.

The read-only CPU registry view exposes only `ProcessorNameString` from Linux
`/proc/cpuinfo`; unknown keys/values fail. Memory totals/availability come from
`/proc/meminfo`, not an invented Windows machine. Commit accounting/cgroup
limits and processor groups are not Windows-compatible; virtual space uses a
conservative low-47-bit budget capped by `RLIMIT_AS`.

The original Lua allocator's dynamically resolved `NtAllocateVirtualMemory`
uses real anonymous Linux reservations and page commitments, with low-address
allocation for its observed request. Managed-region `VirtualQuery` and
decommit/release are connected; recommit preserves existing data or zeroes
decommitted pages as appropriate. Only owned mappings and non-executable
protections are supported. Arbitrary address-space queries, guard pages,
large-page/NUMA policies and general NT/JIT contracts remain open.

`LoadLibraryA`/`GetModuleHandle`/`GetProcAddress` expose only the known authored
kernel/NT/timer ABI modules. Explicit loads/releases track references; this does
not load Windows DLL code or unload the resident import transport. Missing
modules/procedures fail. General Windows DLL loading, SEH/unwind and exception
dispatch, full TEB/PEB/thread behavior, window/OpenGL, audio and Steam services remain
unimplemented here.

Exception-filter registration retains/replaces the native callback and returns
the previous value; native Windows exception dispatch is not connected. The
named mutex binding implements only GE6's singleton presence/lifetime check
with a per-user Linux file lock, not general Wait/ReleaseMutex behavior.

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
API contracts use Microsoft's [Winsock startup](https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsastartup),
[timer requests](https://learn.microsoft.com/en-us/windows/win32/api/timeapi/nf-timeapi-timebeginperiod)
and [NT virtual allocation](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-ntallocatevirtualmemory)
documentation; satisfying a bounded observed contract does not establish complete Windows API parity.
Thread behavior follows the bounded contracts from
[CreateThread](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createthread),
[FLS cleanup](https://learn.microsoft.com/en-us/windows/win32/api/fibersapi/nf-fibersapi-flsfree)
and [legacy debugger naming](https://learn.microsoft.com/en-us/visualstudio/debugger/tips-for-debugging-threads?view=visualstudio).

Acceptance milestones are original CRT completion, original application startup,
renderer/window creation, resource loading and a native GE6 menu/map session.
Mapping or passing an authored ABI fixture does not establish these milestones.
