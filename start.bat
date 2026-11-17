@echo off
REM Starts the relay (new window) and DOSBox running the MASM traffic program.
REM Needs: Node.js 18+ and DOSBox on the PATH (or set DOSBOX=C:\path\to\DOSBox.exe).
setlocal
cd /d "%~dp0"
if "%DOSBOX%"=="" set DOSBOX=dosbox
if not exist relay\node_modules ( pushd relay & call npm install & popd )
start "Traffic relay" cmd /k "cd relay && node server.js --pin 1234"
REM cwd is the project folder, so a relative path avoids quote problems with spaces
"%DOSBOX%" -conf dosbox-traffic.conf -c "mount c platform\dos" -c "c:" -c "traffic.exe"
