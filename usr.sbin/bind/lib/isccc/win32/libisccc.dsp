# Microsoft Developer Studio Project File - Name="libisccc" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=libisccc - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "libisccc.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "libisccc.mak" CFG="libisccc - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "libisccc - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "libisccc - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "libisccc - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "libisccc_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "./" /I "../../../" /I "include" /I "../include" /I "../../../lib/isc/win32" /I "../../../lib/isc/win32/include" /I "../../../lib/dns/win32/include" /I "../../../lib/dns/include" /I "../../../lib/isc/include" /I "../..../lib/dns/sec/openssl/include" /I "../../../lib/dns/sec/dst/include" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "__STDC__" /D "_MBCS" /D "_USRDLL" /D "USE_MD5" /D "OPENSSL" /D "DST_USE_PRIVATE_OPENSSL" /D "LIBISCCC_EXPORTS" /YX /FD /c
# SUBTRACT CPP /X
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 user32.lib advapi32.lib ws2_32.lib ../../isc/win32/Release/libisc.lib /nologo /dll /machine:I386 /out:"../../../Build/Release/libisccc.dll"

!ELSEIF  "$(CFG)" == "libisccc - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "libisccc_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "./" /I "../../../" /I "include" /I "../include" /I "../../../lib/isc/win32" /I "../../../lib/isc/win32/include" /I "../../../lib/dns/win32/include" /I "../../../lib/dns/include" /I "../../../lib/isc/include" /I "../..../lib/dns/sec/openssl/include" /I "../../../lib/dns/sec/dst/include" /D "_DEBUG" /D "WIN32" /D "__STDC__" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "USE_MD5" /D "OPENSSL" /D "DST_USE_PRIVATE_OPENSSL" /D "LIBISCCC_EXPORTS" /FR /YX /FD /GZ /c
# SUBTRACT CPP /X
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 user32.lib advapi32.lib ws2_32.lib ../../isc/win32/debug/libisc.lib /nologo /dll /debug /machine:I386 /out:"../../../Build/Debug/libisccc.dll" /pdbtype:sept

!ENDIF 

# Begin Target

# Name "libisccc - Win32 Release"
# Name "libisccc - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\alist.c
# End Source File
# Begin Source File

SOURCE=..\base64.c
# End Source File
# Begin Source File

SOURCE=..\cc.c
# End Source File
# Begin Source File

SOURCE=..\ccmsg.c
# End Source File
# Begin Source File

SOURCE=.\DLLMain.c
# End Source File
# Begin Source File

SOURCE=..\lib.c
# End Source File
# Begin Source File

SOURCE=..\result.c
# End Source File
# Begin Source File

SOURCE=..\sexpr.c
# End Source File
# Begin Source File

SOURCE=..\symtab.c
# End Source File
# Begin Source File

SOURCE=.\version.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\include\isccc\alist.h
# End Source File
# Begin Source File

SOURCE=..\include\isccc\base64.h
# End Source File
# Begin Source File

SOURCE=..\include\isccc\cc.h
# End Source File
# Begin Source File

SOURCE=..\include\isccc\ccmsg.h
# End Source File
# Begin Source File

SOURCE=..\include\isccc\events.h
# End Source File
# Begin Source File

SOURCE=..\include\isccc\lib.h
# End Source File
# Begin Source File

SOURCE=..\include\isccc\result.h
# End Source File
# Begin Source File

SOURCE=..\include\isccc\sexpr.h
# End Source File
# Begin Source File

SOURCE=..\include\isccc\symtab.h
# End Source File
# Begin Source File

SOURCE=..\include\isccc\symtype.h
# End Source File
# Begin Source File

SOURCE=..\include\isccc\types.h
# End Source File
# Begin Source File

SOURCE=..\include\isccc\util.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# Begin Source File

SOURCE=.\libisccc.def
# End Source File
# End Target
# End Project
