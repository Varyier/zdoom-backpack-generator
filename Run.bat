
@echo off

set APP_PATH=Release\backpack-gen.exe

if exist "x64\Release\backpack-gen.exe" (set APP_PATH=x64\Release\backpack-gen.exe)
if not exist %APP_PATH% ( goto error )

%APP_PATH% ammo-config.txt --decorate-template-file=res\DECORATE.template --sbarinfo-template-file=res\SBARINFO.template -addfile res\MAPINFO -addfile res\MAP01\MAP01 -addfile res\MAP01\TEXTMAP -addfile res\MAP01\ZNODES -addfile res\MAP01\ENDMAP
exit /b 0

:error
echo !!! ERROR: cannot find program executable - build it first
exit /b 1