@echo off
@rem AUTHOR: apc
@rem CREATED: Thu 18th Jan 2001 09:18:08
@rem LAST REVISED: 6th April 2001
@rem LAST REVISED: 22nd May 2002
@rem Batch file to set the path to Default Buildtype,NetWare SDK, CodeWarrior directories 
@rem This file calls buildtype with release as defualt,setnlmsdk.bat, setCodeWar.bat & setmpksdk.bat and MpkBuild with off as default

REM If no parameters are passed, display usage
if "%1" == "" goto Usage
if "%1" == "/?" goto Usage
if "%1" == "/h" goto Usage

REM Display the current settings
if "%1" == "/now" goto now

REM If na is passed, don't set that parameter
if "%1" == "na" goto skip_nlmsdk_msg

:setnwsdk
call setnlmsdk %1
goto skip_nlmsdk_nomsg

:skip_nlmsdk_msg
@echo Retaining NLMSDKBASE=%NLMSDKBASE%

:skip_nlmsdk_nomsg
if "%2" == "" goto err_exit
if "%2" == "na" goto skip_cw_msg

:setcodewar
call setcodewar %2
goto skip_cw_nomsg

:skip_cw_msg
@echo Retaining CODEWAR=%CODEWAR%
goto exit

:skip_cw_nomsg
goto exit

:err_exit
@echo Not Enough Parameters
goto Usage

:now
@echo NLMSDKBASE=%NLMSDKBASE%
@echo CODEWAR=%CODEWAR%
goto exit

:Usage
 @echo on
 @echo "Usage: setnwdef <path to NetWare SDK> <path to CodeWarrior dir>"
 @echo "Usage: setnwdef /now" - To display current setting
 @echo Pass na if you don't want to change a setting
 @echo Ex. setnwbld d:\ndk\nwsdk na
 @echo Ex. setnwbld na d:\codewar

:exit
