
@echo off

rem This file runs makemake program to create a makefile 
rem which can be run remotely at the command line 
rem from the msvc++ IDE.

rem g: must be set up to point to the devo (g:\gdb\mswin) directory
rem c: must be set up to point to the c:\ directory
if not exist g:\gdb goto nog
if not exist c:\gs goto noc

MSVC=c:\msvc20

copy %MSVC%\cl.exe %MSVC%\clsav.exe
copy %MSVC%\link.exe %MSVC%\linksav.exe

copy makemake.exe %MSVC%\cl.exe
copy makemake.exe %MSVC%\link.exe

%MSVC%\vcvars32.bat
msvc gui.mak

copy %MSVC%\clsav.exe %MSVC%\cl.exe
copy %MSVC%\linksav.exe %MSVC%\link.exe

goto done

:nog
echo unable to access directory g:\gdb
net use m: \\canuck\dawn
subst g: m:\gdb\devo
exit

:noc
echo unable to access directory c:\gs
mkdir c:\gs
exit

:done
