@rem $LynxId: lynx.bat,v 1.1 2007/08/01 23:54:17 tom Exp $
@rem Claudio Santambrogio
@ECHO OFF
command /C
set term=vt100
set home=%CD%
set temp=%HOME%\tmp
set lynx_cfg=%HOME%\lynx-demo.cfg
set lynx_lss=%HOME%\opaque.lss
%HOME%\lynx.exe %1 %2 %3 %4 %5
