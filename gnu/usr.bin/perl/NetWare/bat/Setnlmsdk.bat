@echo off
@rem AUTHOR: sgp
@rem CREATED: 24th July 2000
@rem LAST REVISED: 6th April 2001
@rem Batch file to set the path to NetWare SDK
@rem This file is called from SetNWBld.bat. 

if "%1" == "/now" goto now
if "%1" == "" goto Usage
if "%1" == "/?" goto usage
if "%1" == "/h" goto usage

set NLMSDKBASE=%1
echo NLMSDKBASE set to %1

goto exit

:now
@echo NLMSDKBASE=%NLMSDKBASE%
goto exit

:Usage
 @echo on
 @echo "Usage: setnlmsdk <path to NetWare sdk>"
 @echo "Usage: setnlmsdk /now" - To display current setting
 @echo Ex. setnlmsdk e:\sdkcd14\nwsdk

:exit
