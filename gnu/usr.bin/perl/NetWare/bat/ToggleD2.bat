@echo off
@rem AUTHOR: sgp
@rem CREATED: 23rd August 1999
@rem LAST REVISED: 6th April 2001
@rem Batch file to toggle D2 flag for debugging in case of debug build 
@rem and remove in case of release build.
@rem This file is called from BuildType.bat

if "%1" == "" goto Usage

if "%1" == "/now" goto now
if "%1" == "on" goto yes
if "%1" == "off" goto no
if "%1" == "/?" goto usage
if "%1" == "/h" goto usage

Rem Invalid input and so display the help message
goto Usage

:now
if "%USE_D2%" == "" echo USE_D2 is removed, uses /d1
if not "%USE_D2%"  == "" echo USE_D2 is set, uses /d2
goto exit

:yes
Set USE_D2=1
echo ....USE_D2 is set, uses /d2
goto exit

:no
Set USE_D2=
echo ....USE_D2 is removed. uses /d1
goto exit

:Usage
 @echo on
 @echo "Usage: ToggleD2 [on|off]"
 @echo "Usage: ToggleD2 /now" - To display current setting

:exit
