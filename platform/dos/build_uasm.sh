#!/bin/sh
# Cross-assemble on Linux/macOS with UASM (MASM-compatible):  ./build_uasm.sh
uasm -nologo -I. -Icore -Ihw -mz -Fo traffic.exe main.asm
