@echo off
REM Build with Borland TASM 3/4 + TLINK - run inside DOSBox from this folder.
tasm /m2 /i. /icore /ihw main.asm
if errorlevel 1 goto end
tlink /x main.obj, traffic.exe
:end
