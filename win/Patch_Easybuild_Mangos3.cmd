@echo off
:main
if not exist ..\dep\.git goto dep:
if not exist ..\src\realmd\.git goto realm:
goto endpoint:

:dep
echo Patching Dep
copy Patch_Easybuild_Mangos3.cmd ..\dep\.git
goto main:

:realm
echo Patching Realm
copy Patch_Easybuild_Mangos3.cmd ..\src\realmd\.git
goto main:

:endpoint