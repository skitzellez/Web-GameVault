@echo off
rem Copyright (c) 2026 Skitzelles - part of Game Vault, https://github.com/skitzellez/xbox-game-vault
rem PolyForm Noncommercial License 1.0.0: not for sale, and this notice stays in every copy.
rem Builds pkg\WebBrowser.exe, then dist\WebForGameVault_x64.appx (signed with Game Vault's certificate).
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>nul
cd /d "%~dp0"
set SDK=C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64
set GV=%~dp0..\GameVault
if not exist out mkdir out
cl /nologo /utf-8 /std:c++20 /EHsc /O2 /W3 /MD /DNOMINMAX /DUNICODE /D_UNICODE /DWINAPI_FAMILY=WINAPI_FAMILY_APP /D__WRL_NO_DEFAULT_LIB__ /Ideps\cppwinrt /Fo:out\ /Fe:pkg\WebBrowser.exe src\main.cpp /link /APPCONTAINER /SUBSYSTEM:WINDOWS /LIBPATH:"%VCToolsInstallDir%lib\x64\store" /NODEFAULTLIB:kernel32.lib /NODEFAULTLIB:ole32.lib WindowsApp.lib || exit /b 1
echo BUILD OK
if exist stage rmdir /s /q stage
mkdir stage\Assets\Licenses
copy /y pkg\AppxManifest.xml stage\ >nul
copy /y pkg\WebBrowser.exe stage\ >nul || exit /b 1
copy /y pkg\Microsoft.Web.WebView2.Core.dll stage\ >nul || exit /b 1
copy /y pkg\Assets\*.png stage\Assets\ >nul
copy /y deps\webview2\LICENSE.txt stage\Assets\Licenses\WebView2-LICENSE.txt >nul
if not exist dist mkdir dist
if exist dist\WebForGameVault_x64.appx del dist\WebForGameVault_x64.appx
"%SDK%\makeappx.exe" pack /o /d stage /p dist\WebForGameVault_x64.appx || exit /b 1
if not defined PFX_PASSWORD for /f "usebackq delims=" %%p in (`powershell -NoProfile -ExecutionPolicy Bypass -File "%GV%\pfx_password.ps1"`) do set "PFX_PASSWORD=%%p"
if not defined PFX_PASSWORD (echo the certificate password could not be read & exit /b 1)
"%SDK%\signtool.exe" sign /fd SHA256 /a /f "%GV%\cert\GameVault.pfx" /p %PFX_PASSWORD% dist\WebForGameVault_x64.appx || exit /b 1
rmdir /s /q stage
echo PACKAGE OK
