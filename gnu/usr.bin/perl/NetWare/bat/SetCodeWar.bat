@echo off
@rem AUTHOR: sgp & apc
@rem CREATED: 24th July 2000
@rem LAST REVISED: 6th April 2001
@rem LAST REVISED: 22nd May 2002
@rem AUTHOR: apc
@rem Batch file to set the path to CodeWarrior directories
@rem This file is called from SetNWBld.bat.

if "%1" == "/now" goto now
if "%1" == "" goto Usage
if "%1" == "/?" goto usage
if "%1" == "/h" goto usage

set CODEWAR=%1
ECHO CODEWAR=%1

call buildtype r
@echo Buildtype set to Release type

set MWCIncludes=%1\include
@echo MWCIncludes=%1\include
set MWLibraries=%1\lib
@echo MWLibraries=%1\lib
set MWLibraryFiles=%1\lib\nwpre.obj;%1\lib\mwcrtld.lib
@echo MWLibraryFiles=%1\lib\nwpre.obj;%1\lib\mwcrtld.lib

set PATH=%PATH%;%1\bin;
@echo PATH=%PATH%;%1\bin;

goto exit

:now
@echo CODEWAR=%CODEWAR%
goto exit

:Usage
 @echo on
 @echo "Usage: setCodeWar <Path to CodeWarrior binaries>"
 @echo "Usage: setCodeWar /now" - To display current setting
 @echo Ex. setCodeWar d:\CodeWar

:exit
