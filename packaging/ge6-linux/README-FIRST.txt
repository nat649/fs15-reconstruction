FS15 / GIANTS ENGINE 6 - PORTABLE LINUX TEST
==========================================

The launcher sources and CI runtime artifact contain no original game files.
A locally prepared PRIVATE test archive also contains the sender's original
FS15 resources. Keep that archive private; do not publish it as a GitHub release.

REQUIREMENTS

Linux x86_64 with glibc 2.31 or newer, Python 3.9+, working desktop OpenGL/GLX
and X11 (an Xorg session, or working XWayland with a nonzero refresh rate).
This build uses the host's graphics driver. No Wine, Proton, container, FUSE,
bundled glibc, replacement renderer or replacement physics engine is required.

Debian 11/12/13 and Arch Linux are the automated runtime-test targets. Other
glibc distributions meeting the requirements may work but are not all validated.
Alpine/musl, 32-bit Linux and ARM are NOT supported by this x86_64 ELF. Non-FHS
environments and every Wayland/driver combination are not covered by the tests.

The archive includes its C++ runtime, OpenSSL crypto and PNG/zlib code in the
binary. It still needs the system's X11, RandR and OpenGL libraries. Do not copy
Ubuntu graphics drivers or glibc into the archive; use your distribution's driver.

DEBIAN / UBUNTU AND DERIVATIVES

sudo apt-get update
sudo apt-get install python3 libx11-6 libxrandr2 libgl1 x11-xserver-utils

For audible output install libpulse0 and libasound2 (Debian 11/12), or
libasound2t64 (Debian 13 / Ubuntu 24.04), with a working audio server/device.
Use --silent to test without audible audio.

ARCH / ENDEAVOUROS AND ARCH DERIVATIVES

sudo pacman -Syu --needed python libx11 libxrandr libglvnd xorg-xrandr alsa-lib libpulse

Keep your normal Mesa or NVIDIA/AMD graphics driver installed. These commands
are instructions only; the launcher never runs sudo or installs packages itself.

START A PREPARED PRIVATE ARCHIVE

1. Extract the ENTIRE archive to a writable folder on a Linux filesystem.
2. Open a terminal in the extracted folder.
3. Run:

   sh start-ge6.sh

The launcher detects the distribution, checks glibc and libraries, and suggests
the relevant package command if dependencies are missing. Missing X11, a
zero-Hz virtual mode or an incompatible runtime produces an explicit error.
It does not invent a monitor mode or automatically change system settings.

The default isolated profile is a 1280x720 window, MSAA off, English, with gamepad
input disabled. Your normal FS15 save directory is not used. Settings are in
profile/. No VM software-rendering or Mesa optimization flags are forced.

Click through the three introduction videos. Choose Career, an empty slot,
Easy, Bjornholm and Start. After loading finishes, click Start again. Enter
dismisses the help; mouse motion changes the view; Tab selects a starting vehicle.
W/S and A/D are the original movement controls. Reliable driving, steering,
braking, farming, saves and full Windows behavior parity remain unverified.

OPTIONAL COMMANDS

sh start-ge6.sh --silent        # explicit silent audio backend
sh start-ge6.sh --verify        # verify every packaged file; do not start GE6
sh start-ge6.sh --check         # check local platform/libraries/display only
sh start-ge6.sh --seconds 300   # bounded five-minute diagnostic

Close the game to finish. If quitting fails, use Ctrl+C in the terminal.
This experimental runtime may stop at an unsupported native call or fault.
A successful preflight or bounded run is not proof of complete gameplay.

RESULTS AND PROVENANCE

After the test, inspect and return the newest test-results/ folder. It contains
startup.log, game-log.txt and report.json. Nothing is uploaded automatically.
Report your distribution, GPU/driver, whether the menu/map/vehicle appeared,
and whether movement, steering and braking actually worked. A screenshot helps.
The engine journal contains the actual OpenGL renderer; report.json records
the actual Linux kernel and distribution. The Windows version in the engine
journal is its authored OS-ABI compatibility response.

BUILD.json identifies the compiled host source and build baseline; ABI.json
records binary hashes, direct shared dependencies and required glibc versions.
The original GE6 instruction image is the existing Local GE6 image with its
previous platform-bootstrap modification. The Linux loader adds no game
instruction patches. Original assets and geometry are used directly.
Steam services and licensed DLC are unavailable. This is not a complete GE6
source decompilation/recompilation or a fully playable Linux game release.

The host is authored code; OpenAL Soft is third-party middleware. Dependency
notices and full OpenAL Soft static relinking materials are included in
licenses/ and ge6-audio-relink/. See ge6-audio-relink/AUDIO_DEPENDENCY.md.
