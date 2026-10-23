#!/bin/sh
# Run beside main.cpp.o and libopenal.a on Linux x86_64 (Ubuntu 24.04 toolchain).
# Replace libopenal.a with a compatible modified OpenAL Soft build if desired.
set -eu
cd "$(dirname "$0")"
mode=static
if [ -f link-mode.txt ]; then mode=$(cat link-mode.txt); fi
case "$mode" in
  static) set -- -static ;;
  dynamic) set -- ;;
  x11) set -- -lX11 -lXrandr -lGL ;;
  x11-portable) set -- -static-libstdc++ -static-libgcc -Wl,-Bstatic -lcrypto -lpng -lz -Wl,-Bdynamic -lX11 -lXrandr -lGL ;;
  *) printf '%s\n' 'Unknown relink mode' >&2; exit 1 ;;
esac
case "$mode" in
  x11-portable) c++ main.cpp.o libopenal.a "$@" -latomic -ldl -lpthread -lm -o FS15GE6LinuxBoot-relinked ;;
  *) c++ main.cpp.o libopenal.a "$@" -lcrypto -lpng -lz -latomic -ldl -lpthread -lm -o FS15GE6LinuxBoot-relinked ;;
esac
