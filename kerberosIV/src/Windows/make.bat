@echo off

pushd ..\appl\voodoo
nmake -F voodoo.mak
popd

for %%f in (roken des krb kclient ) do echo %%f & copy ..\lib\%%f\Release\*.dll .\bin
for %%f in (krbmanager voodoo) do echo %%f & copy ..\appl\%%f\Release\%%f.exe .\bin
