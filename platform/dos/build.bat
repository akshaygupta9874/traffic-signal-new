@echo off
REM Build with MASM 6.x  (ml.exe + link.exe) - run inside DOSBox from this folder.
REM Include directories are given so MAIN.ASM can use plain file names.
ml /nologo /c /Zm /I. /Icore /Ihw main.asm
if errorlevel 1 goto end
link /nologo main.obj,traffic.exe,nul,,,
:end
