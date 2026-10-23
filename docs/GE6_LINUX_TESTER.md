# Original GE6 Linux desktop tester

This package executes the prepared original FS15 / GIANTS Engine 6 x64
instructions through the authored Linux OS and Microsoft-ABI transport.
It uses neither Wine nor Proton. It is separate from ReFarm and the historical
recovered frontend tester. It is **experimental**, with complete gameplay and
Windows parity still unverified.

## Desktop requirements

- Linux x86_64, glibc 2.31 or newer, and Python 3.9 or newer.
- A graphical Xorg session, or working XWayland with GLX and a nonzero actual
  display refresh rate. Native Wayland-only operation is not implemented.
- A working OpenGL compatibility driver from the distribution / GPU vendor.
- Space for the archive and approximately 2.8 GB of extracted files.

The portable binary is built on Debian 11. CI runs the **same binary** against
Debian 11, Debian 12, Debian 13 and Arch Linux userspace libraries, including
actual X11/OpenGL framebuffer and input checks. These are authored fixtures;
original game resources are not uploaded to CI. They do not establish a full
original gameplay session on each distribution or every GPU driver.

The glibc baseline and host-library strategy also target compatible derivatives.
This does not cover every Linux distribution: ARM, 32-bit, musl-only systems
(such as stock Alpine), non-FHS systems and all compositor/driver combinations
are outside the validated scope.

Debian / Ubuntu desktop dependencies:

```sh
sudo apt-get update
sudo apt-get install python3 libx11-6 libxrandr2 libgl1 x11-xserver-utils
```

Arch / compatible desktop dependencies:

```sh
sudo pacman -Syu --needed python libx11 libxrandr libglvnd xorg-xrandr alsa-lib libpulse
```

Install the appropriate Mesa or proprietary OpenGL driver for the actual GPU.
The archive does not bundle another distribution's graphics driver or glibc.
ALSA/PulseAudio libraries and a working device/server are needed for sound;
use the explicit silent option below to isolate an audio startup failure.

## Run a prepared private archive

Extract the complete `.tar.gz`, open a terminal inside the extracted directory,
and run:

```sh
sh start-ge6.sh --check
sh start-ge6.sh
```

The first command checks file hashes, architecture, actual glibc, dynamic
libraries and X11 refresh information without starting GE6. The second creates
an isolated `profile/`, defaults to a 1280 x 720 window and runs original GE6.
Click through the three introductions, then select Career. Profile settings can
be changed in `profile/game.xml` after the first launch.

For a bounded silent test or complete archive verification:

```sh
sh start-ge6.sh --silent --seconds 120
sh start-ge6.sh --verify
```

The launcher inherits the actual desktop GPU settings; it does not force
software rendering or the VM's diagnostic shader options. If the game cannot
quit, press Ctrl+C in its terminal. Logs and system/build details are stored
locally in `test-results/<timestamp>/`; review them before sharing them.
Nothing is uploaded automatically.

`BUILD.json` pins the source commit and build baseline. `ABI.json` records ELF
hashes, direct shared dependencies and maximum required glibc symbols.
`PACKAGE-MANIFEST.json` records the prepared private package's file inventory.
The archive includes OpenAL source, objects, static archive, relinking script
and dependency notices. See [audio dependency and relinking](../src/ge6_linux/AUDIO_DEPENDENCY.md).

## What is verified

The [four-distribution CI run](https://github.com/nat649/fs15-reconstruction/actions/runs/39960605291)
passed on Debian 11, 12, 13 and Arch. The package's executable has exactly the
same SHA-256 as the executable checked there. The portable launcher also passed
a 90-second private original-code test with a 1280 x 720 Xorg window, actual
GLX context, 47 presented frames and original vehicle-type loading. Audio was
explicitly silent and the controller stopped the process at the time limit;
normal game shutdown and a full play session were not tested in that run.
See [machine-readable evidence](../analysis/ge6-linux-distros-validation.json).

Original GE6 has rendered its original menu, mission loading, Bjornholm scene,
player HUD and starting combine in the isolated Linux VM. Original mouse-look
and keyboard menu/help/vehicle selection have been observed. The world test
used a 320 x 180 software-rendering diagnostic; it is not evidence of normal
performance on a desktop. Sustained walking/driving, farming, reliable saves,
audible gameplay and Windows parity remain open. See
[original execution evidence](GE6_LINUX_NATIVE_BOOT.md).

The private Local image retains its previously prepared Windows platform
bootstrap modification. This Linux packaging work adds no instruction patch.
It does not mean the entire original GE6 has been decompiled or recompiled.

## Reproduce the authored portable runtime

The [desktop distro workflow](../.github/workflows/ge6-linux-portable.yml)
builds pinned OpenSSL 3.5.9 and OpenAL Soft on Debian 11, statically links the
C++/crypto/PNG dependencies, audits ELF dependencies and tests the runtime on
four userspaces. Its `ge6-linux-portable-x86_64` artifact contains **no original
executable, scripts or assets**.

The public `scripts/package_ge6_friend.py` prepares and independently verifies
a private archive from that runtime and already prepared local original inputs.
Its output stays under ignored `build/`; do not commit or publicly publish game
payloads. A normal public runtime artifact alone cannot start the original game.
