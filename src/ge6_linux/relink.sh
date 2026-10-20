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
  *) printf '%s\n' 'Unknown relink mode' >&2; exit 1 ;;
esac
c++ main.cpp.o libopenal.a "$@" -lcrypto -lpng -lz -latomic -ldl -lpthread -lm -o FS15GE6LinuxBoot-relinked
