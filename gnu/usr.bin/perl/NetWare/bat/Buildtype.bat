@echo off
@rem AUTHOR: sgp
@rem CREATED: 24th July 2000
@rem LAST REVISED: 6th April 2001
@rem Batch file to set debug/release build and toggle D2 flag for 
@rem debugging in case of debug build.
@rem This file calls ToggleD2.bat which switches b/n d2 & d1 flags

if "%1" == "" goto Usage
if "%1" == "/now" goto now
if "%1" == "/?" goto usage
if "%1" == "/h" goto usage

if "%1" == "r" goto set_type_rel
if "%1" == "R" goto set_type_rel

if "%1" == "d" goto set_type_dbg
if "%1" == "D" goto set_type_dbg

Rem Invalid input and so display the help message
goto Usage
 
:set_type_rel
set MAKE_TYPE=Release
echo ....Build set to %MAKE_TYPE%
goto set_d2_off

:set_type_dbg
set MAKE_TYPE=Debug
echo ....Build set to %MAKE_TYPE%
if "%2" == "" goto set_d2_off
call ToggleD2 %2

goto exit

:set_d2_off
call ToggleD2 off
goto exit

:now
if "%MAKE_TYPE%" == "" echo MAKE_TYPE is not set, hence it defaults to Release build
if not "%MAKE_TYPE%" == "" echo Current build type is - %MAKE_TYPE%
call ToggleD2 /now
goto exit

:Usage
 @echo on
 @echo "Usage: buildtype r/R|d/D [on/off]"  
 @echo      on/off - Toggling only for D2 flag during debug build
 @echo "Usage: buildtype /now"  - To display current setting
 @echo Ex. buildtype d on

:exit
