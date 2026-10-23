# Linux audio dependency and relinking

The native instruction transport calls unmodified **OpenAL Soft 1.24.3** through
authored Microsoft x64 to System V ABI adapters. This is third-party Linux audio
middleware, not a decompilation of the original OpenAL DLL or of GE6 audio code.

Source: https://openal-soft.org/openal-releases/openal-soft-1.24.3.tar.bz2

SHA-256: `cb5e6197a1c0da0edcf2a81024953cc8fa8545c3b9474e48c852af709d587892`.

OpenAL Soft is distributed under LGPL-2.0-or-later, with the upstream exceptions
and per-file notices. The accompanying complete source directory contains
`COPYING`, the bundled fmt license, and the original source notices. It has not
been modified. The installed archive contains the fmt objects as well.

For this diagnostic's static build, `main.cpp.o`, `libopenal.a`, the complete
OpenAL Soft source and `relink.sh` are supplied together. On Ubuntu 24.04 x86_64,
install `g++`, `cmake`, `libssl-dev`, `libasound2-dev`, `libpulse-dev` and `libpng-dev`.
Run `sh relink.sh` to relink the diagnostic, including with a compatible modified
OpenAL Soft archive. The adapters' source is in the public repository at the
artifact's source commit. No restriction is imposed on modifying this library
or reverse engineering this diagnostic to debug those modifications.

The executable also links the distribution libpng/zlib for PNG frames in PE
icon resources. Their copyright/license notices accompany the boot artifact.
The relink script links those distribution development archives as well.

The X11/GLX artifact supplies the same OpenAL source and relinking objects.
Its `link-mode.txt` selects dynamic X11, RandR and OpenGL libraries; install
`libx11-dev`, `libxrandr-dev` and `libgl-dev` before relinking on Ubuntu 24.04.
X11/RandR provide host display/window services and Mesa/GLX provides native
OpenGL. Neither is recovered GE6 code. The artifact includes distribution
copyright notices for these dependencies.

To rebuild OpenAL, use its supplied source with `-DLIBTYPE=STATIC`,
`-DALSOFT_UTILS=OFF -DALSOFT_EXAMPLES=OFF -DALSOFT_TESTS=OFF` and the backend
options in the transport CMake file; copy the resulting `libopenal.a` here.

ALSA and PulseAudio backends are compiled in and load their normal host shared
libraries at runtime. A fully static diagnostic still needs those libraries
and a working device/server for audible desktop output. The isolated VM and
authored CI fixtures explicitly select `ALSOFT_DRIVERS=null`; loopback tests
render synthetic PCM into memory. Neither proves audible gameplay.

Only the Windows router selectors `Generic Software` and `Generic Hardware`
map to the Linux default device, with a trace. Explicit device names are passed
unchanged and invalid names fail. No success, device, context or audio sample is
fabricated. Extensions are not exposed as unsafe System V function pointers.

## Portable desktop variant

`FS15_GE6_PORTABLE=ON` keeps X11/RandR/OpenGL and glibc dynamic, and statically
links libstdc++, libgcc, libpng, zlib and crypto dependencies. The desktop
workflow builds on Debian 11 and audits the resulting ELF for glibc <= 2.31
and absence of distribution-specific C++/PNG/crypto shared requirements.
It does not bundle GPU drivers or glibc.

The portable workflow builds unmodified OpenSSL **3.5.9** from its official
release archive, SHA-256
`603f5602e2eef00d77fbd429d34dcd5822bb301757a1bc9cdb24c670f1eb859a`.
OpenSSL 3 is required by the host's actual provider/random API. Its Apache 2.0
license accompanies the artifact. The exact `libcrypto.a` is supplied beside
the OpenAL relinking materials, and `relink.sh` selects it for `x11-portable`.
To relink that artifact, install a C++ toolchain plus PNG/zlib and X11/RandR/GL
development libraries, then run `sh relink.sh` from its directory. Rebuilding
OpenAL from the supplied complete source still allows replacing `libopenal.a`.
The public source at `BUILD.json`'s commit contains the authored transport.
