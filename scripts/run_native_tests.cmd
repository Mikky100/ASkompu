@echo off
set "PATH=%USERPROFILE%\.platformio\packages\toolchain-gccmingw32\bin;%PATH%"
"%~dp0..\.pio\build\native\program.exe"
