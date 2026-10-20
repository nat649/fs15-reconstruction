#!/bin/sh
# Run beside main.cpp.o and libopenal.a on Linux x86_64 (Ubuntu 24.04 toolchain).
# Replace libopenal.a with a compatible modified OpenAL Soft build if desired.
set -eu
cd "$(dirname "$0")"
c++ -static main.cpp.o libopenal.a -lcrypto -latomic -ldl -lpthread -lm -o FS15GE6LinuxBoot-relinked
