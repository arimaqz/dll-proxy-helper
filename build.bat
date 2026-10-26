@echo off
setlocal
cd /d "%~dp0"
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist bin mkdir bin
cl /nologo /O2 /EHsc helper.cpp /Fe:bin\helper.exe
if errorlevel 1 exit /b 1
cl /nologo /O2 /EHsc procdll.cpp /Fe:bin\procdll.exe
if errorlevel 1 exit /b 1
endlocal
