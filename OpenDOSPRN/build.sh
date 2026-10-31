#!/bin/sh
# Cross-compile 32-bit Windows binary with MinGW-w64
set -e
cd "$(dirname "$0")"
mkdir -p build
i686-w64-mingw32-windres res/app.rc -O coff -o build/app.o
i686-w64-mingw32-g++ -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Wno-cast-function-type \
  -municode -mwindows -static -static-libgcc -static-libstdc++ \
  src/*.cpp build/app.o -o build/OpenDOSPRN.exe \
  -lcomctl32 -lcomdlg32 -lwinspool -lshell32 -lole32 -ladvapi32 -lgdi32 -luser32
echo built build/OpenDOSPRN.exe
