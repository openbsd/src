# Microsoft Developer Studio Project File - Name="named" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=named - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "named.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "named.mak" CFG="named - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "named - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "named - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "named - Win32 Release"

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
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "./" /I "../../../" /I "../win32/include" /I "../include" /I "../../../lib/isc/win32" /I "../../../lib/isc/win32/include" /I "../../../lib/isc/include" /I "../../../lib/dns/win32/include" /I "../../../lib/dns/include" /I "../../../lib/dns/sec/dst/include" /I "../../../lib/isccc/include" /I "../../../lib/lwres/win32/include" /I "../../../lib/lwres/include" /I "../../../lib/isccfg/include" /D "WIN32" /D "NDEBUG" /D "__STDC__" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 user32.lib advapi32.lib kernel32.lib ws2_32.lib ../../../lib/isc/win32/Release/libisc.lib ../../../lib/dns/win32/Release/libdns.lib ../../../lib/isccc/win32/Release/libisccc.lib ../../../lib/lwres/win32/Release/liblwres.lib ../../../lib/isccfg/win32/Release/libisccfg.lib /nologo /subsystem:console /machine:I386 /out:"../../../Build/Release/named.exe"

!ELSEIF  "$(CFG)" == "named - Win32 Debug"

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
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "./" /I "../../../" /I "../win32/include" /I "../include" /I "../../../lib/isc/win32" /I "../../../lib/isc/win32/include" /I "../../../lib/isc/include" /I "../../../lib/dns/win32/include" /I "../../../lib/dns/include" /I "../../../lib/dns/sec/dst/include" /I "../../../lib/isccc/include" /I "../../../lib/lwres/win32/include" /I "../../../lib/lwres/include" /I "../../../lib/isccfg/include" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /D "i386" /FR /FD /GZ /c
# SUBTRACT CPP /X /YX
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 user32.lib advapi32.lib kernel32.lib ws2_32.lib ../../../lib/isc/win32/Debug/libisc.lib ../../../lib/dns/win32/Debug/libdns.lib ../../../lib/isccc/win32/Debug/libisccc.lib ../../../lib/lwres/win32/Debug/liblwres.lib ../../../lib/isccfg/win32/Debug/libisccfg.lib /nologo /subsystem:console /map /debug /machine:I386 /out:"../../../Build/Debug/named.exe" /pdbtype:sept

!ENDIF 

# Begin Target

# Name "named - Win32 Release"
# Name "named - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\aclconf.c
# End Source File
# Begin Source File

SOURCE=..\client.c
# End Source File
# Begin Source File

SOURCE=..\config.c
# End Source File
# Begin Source File

SOURCE=..\control.c
# End Source File
# Begin Source File

SOURCE=..\controlconf.c
# End Source File
# Begin Source File

SOURCE=..\interfacemgr.c
# End Source File
# Begin Source File

SOURCE=..\listenlist.c
# End Source File
# Begin Source File

SOURCE=..\log.c
# End Source File
# Begin Source File

SOURCE=..\logconf.c
# End Source File
# Begin Source File

SOURCE=..\lwaddr.c
# End Source File
# Begin Source File

SOURCE=..\lwdclient.c
# End Source File
# Begin Source File

SOURCE=..\lwderror.c
# End Source File
# Begin Source File

SOURCE=..\lwdgabn.c
# End Source File
# Begin Source File

SOURCE=..\lwdgnba.c
# End Source File
# Begin Source File

SOURCE=..\lwdgrbn.c
# End Source File
# Begin Source File

SOURCE=..\lwdnoop.c
# End Source File
# Begin Source File

SOURCE=..\lwresd.c
# End Source File
# Begin Source File

SOURCE=..\lwsearch.c
# End Source File
# Begin Source File

SOURCE=..\main.c
# End Source File
# Begin Source File

SOURCE=..\notify.c
# End Source File
# Begin Source File

SOURCE=.\ntservice.c
# End Source File
# Begin Source File

SOURCE=.\os.c
# End Source File
# Begin Source File

SOURCE=..\query.c
# End Source File
# Begin Source File

SOURCE=..\server.c
# End Source File
# Begin Source File

SOURCE=..\sortlist.c
# End Source File
# Begin Source File

SOURCE=..\tkeyconf.c
# End Source File
# Begin Source File

SOURCE=..\tsigconf.c
# End Source File
# Begin Source File

SOURCE=..\update.c
# End Source File
# Begin Source File

SOURCE=..\xfrout.c
# End Source File
# Begin Source File

SOURCE=..\zoneconf.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\include\named\aclconf.h
# End Source File
# Begin Source File

SOURCE=..\include\named\client.h
# End Source File
# Begin Source File

SOURCE=..\include\named\config.h
# End Source File
# Begin Source File

SOURCE=..\include\named\globals.h
# End Source File
# Begin Source File

SOURCE=..\include\named\interfacemgr.h
# End Source File
# Begin Source File

SOURCE=..\include\named\listenlist.h
# End Source File
# Begin Source File

SOURCE=..\include\named\log.h
# End Source File
# Begin Source File

SOURCE=..\include\named\logconf.h
# End Source File
# Begin Source File

SOURCE=..\include\named\lwaddr.h
# End Source File
# Begin Source File

SOURCE=..\include\named\lwdclient.h
# End Source File
# Begin Source File

SOURCE=..\include\named\lwresd.h
# End Source File
# Begin Source File

SOURCE=..\include\named\lwsearch.h
# End Source File
# Begin Source File

SOURCE=..\include\named\main.h
# End Source File
# Begin Source File

SOURCE=..\include\named\notify.h
# End Source File
# Begin Source File

SOURCE=.\include\named\ntservice.h
# End Source File
# Begin Source File

SOURCE=..\include\named\omapi.h
# End Source File
# Begin Source File

SOURCE=.\include\named\os.h
# End Source File
# Begin Source File

SOURCE=..\include\named\query.h
# End Source File
# Begin Source File

SOURCE=..\include\named\server.h
# End Source File
# Begin Source File

SOURCE=..\include\named\sortlist.h
# End Source File
# Begin Source File

SOURCE=..\include\named\tkeyconf.h
# End Source File
# Begin Source File

SOURCE=..\include\named\tsigconf.h
# End Source File
# Begin Source File

SOURCE=..\include\named\types.h
# End Source File
# Begin Source File

SOURCE=..\include\named\update.h
# End Source File
# Begin Source File

SOURCE=..\include\named\xfrout.h
# End Source File
# Begin Source File

SOURCE=..\include\named\zoneconf.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# End Target
# End Project
