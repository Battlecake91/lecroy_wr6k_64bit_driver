@echo off
setlocal

if "%VSCMD_ARG_TGT_ARCH%"=="" (
  echo Run this from an x64 Native Tools Command Prompt for Visual Studio.
  exit /b 1
)

if not exist build mkdir build

cl /nologo /W4 /O2 /D_CRT_SECURE_NO_WARNINGS ^
  /Fe:build\lecdiag.exe lecdiag.c

exit /b %errorlevel%
