# Microsoft Developer Studio Generated NMAKE File, Based on named.dsp
!IF "$(CFG)" == ""
CFG=named - Win32 Debug
!MESSAGE No configuration specified. Defaulting to named - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "named - Win32 Release" && "$(CFG)" != "named - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
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
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "named - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release

ALL : "..\..\..\Build\Release\named.exe"


CLEAN :
	-@erase "$(INTDIR)\aclconf.obj"
	-@erase "$(INTDIR)\client.obj"
	-@erase "$(INTDIR)\config.obj"
	-@erase "$(INTDIR)\control.obj"
	-@erase "$(INTDIR)\controlconf.obj"
	-@erase "$(INTDIR)\interfacemgr.obj"
	-@erase "$(INTDIR)\listenlist.obj"
	-@erase "$(INTDIR)\log.obj"
	-@erase "$(INTDIR)\logconf.obj"
	-@erase "$(INTDIR)\lwaddr.obj"
	-@erase "$(INTDIR)\lwdclient.obj"
	-@erase "$(INTDIR)\lwderror.obj"
	-@erase "$(INTDIR)\lwdgabn.obj"
	-@erase "$(INTDIR)\lwdgnba.obj"
	-@erase "$(INTDIR)\lwdgrbn.obj"
	-@erase "$(INTDIR)\lwdnoop.obj"
	-@erase "$(INTDIR)\lwresd.obj"
	-@erase "$(INTDIR)\lwsearch.obj"
	-@erase "$(INTDIR)\main.obj"
	-@erase "$(INTDIR)\notify.obj"
	-@erase "$(INTDIR)\ntservice.obj"
	-@erase "$(INTDIR)\os.obj"
	-@erase "$(INTDIR)\query.obj"
	-@erase "$(INTDIR)\server.obj"
	-@erase "$(INTDIR)\sortlist.obj"
	-@erase "$(INTDIR)\tkeyconf.obj"
	-@erase "$(INTDIR)\tsigconf.obj"
	-@erase "$(INTDIR)\update.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\xfrout.obj"
	-@erase "$(INTDIR)\zoneconf.obj"
	-@erase "..\..\..\Build\Release\named.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MD /W3 /GX /O2 /I "./" /I "../../../" /I "../win32/include" /I "../include" /I "../../../lib/isc/win32" /I "../../../lib/isc/win32/include" /I "../../../lib/isc/include" /I "../../../lib/dns/win32/include" /I "../../../lib/dns/include" /I "../../../lib/dns/sec/dst/include" /I "../../../lib/isccc/include" /I "../../../lib/lwres/win32/include" /I "../../../lib/lwres/include" /I "../../../lib/isccfg/include" /D "WIN32" /D "NDEBUG" /D "__STDC__" /D "_CONSOLE" /D "_MBCS" /Fp"$(INTDIR)\named.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

RSC=rc.exe
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\named.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=user32.lib advapi32.lib kernel32.lib ws2_32.lib ../../../lib/isc/win32/Release/libisc.lib ../../../lib/dns/win32/Release/libdns.lib ../../../lib/isccc/win32/Release/libisccc.lib ../../../lib/lwres/win32/Release/liblwres.lib ../../../lib/isccfg/win32/Release/libisccfg.lib /nologo /subsystem:console /incremental:no /pdb:"$(OUTDIR)\named.pdb" /machine:I386 /out:"../../../Build/Release/named.exe" 
LINK32_OBJS= \
	"$(INTDIR)\aclconf.obj" \
	"$(INTDIR)\client.obj" \
	"$(INTDIR)\config.obj" \
	"$(INTDIR)\control.obj" \
	"$(INTDIR)\controlconf.obj" \
	"$(INTDIR)\interfacemgr.obj" \
	"$(INTDIR)\listenlist.obj" \
	"$(INTDIR)\log.obj" \
	"$(INTDIR)\logconf.obj" \
	"$(INTDIR)\lwaddr.obj" \
	"$(INTDIR)\lwdclient.obj" \
	"$(INTDIR)\lwderror.obj" \
	"$(INTDIR)\lwdgabn.obj" \
	"$(INTDIR)\lwdgnba.obj" \
	"$(INTDIR)\lwdgrbn.obj" \
	"$(INTDIR)\lwdnoop.obj" \
	"$(INTDIR)\lwresd.obj" \
	"$(INTDIR)\lwsearch.obj" \
	"$(INTDIR)\main.obj" \
	"$(INTDIR)\notify.obj" \
	"$(INTDIR)\ntservice.obj" \
	"$(INTDIR)\os.obj" \
	"$(INTDIR)\query.obj" \
	"$(INTDIR)\server.obj" \
	"$(INTDIR)\sortlist.obj" \
	"$(INTDIR)\tkeyconf.obj" \
	"$(INTDIR)\tsigconf.obj" \
	"$(INTDIR)\update.obj" \
	"$(INTDIR)\xfrout.obj" \
	"$(INTDIR)\zoneconf.obj"

"..\..\..\Build\Release\named.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "named - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

ALL : "..\..\..\Build\Debug\named.exe" "$(OUTDIR)\named.bsc"


CLEAN :
	-@erase "$(INTDIR)\aclconf.obj"
	-@erase "$(INTDIR)\aclconf.sbr"
	-@erase "$(INTDIR)\client.obj"
	-@erase "$(INTDIR)\client.sbr"
	-@erase "$(INTDIR)\config.obj"
	-@erase "$(INTDIR)\config.sbr"
	-@erase "$(INTDIR)\control.obj"
	-@erase "$(INTDIR)\control.sbr"
	-@erase "$(INTDIR)\controlconf.obj"
	-@erase "$(INTDIR)\controlconf.sbr"
	-@erase "$(INTDIR)\interfacemgr.obj"
	-@erase "$(INTDIR)\interfacemgr.sbr"
	-@erase "$(INTDIR)\listenlist.obj"
	-@erase "$(INTDIR)\listenlist.sbr"
	-@erase "$(INTDIR)\log.obj"
	-@erase "$(INTDIR)\log.sbr"
	-@erase "$(INTDIR)\logconf.obj"
	-@erase "$(INTDIR)\logconf.sbr"
	-@erase "$(INTDIR)\lwaddr.obj"
	-@erase "$(INTDIR)\lwaddr.sbr"
	-@erase "$(INTDIR)\lwdclient.obj"
	-@erase "$(INTDIR)\lwdclient.sbr"
	-@erase "$(INTDIR)\lwderror.obj"
	-@erase "$(INTDIR)\lwderror.sbr"
	-@erase "$(INTDIR)\lwdgabn.obj"
	-@erase "$(INTDIR)\lwdgabn.sbr"
	-@erase "$(INTDIR)\lwdgnba.obj"
	-@erase "$(INTDIR)\lwdgnba.sbr"
	-@erase "$(INTDIR)\lwdgrbn.obj"
	-@erase "$(INTDIR)\lwdgrbn.sbr"
	-@erase "$(INTDIR)\lwdnoop.obj"
	-@erase "$(INTDIR)\lwdnoop.sbr"
	-@erase "$(INTDIR)\lwresd.obj"
	-@erase "$(INTDIR)\lwresd.sbr"
	-@erase "$(INTDIR)\lwsearch.obj"
	-@erase "$(INTDIR)\lwsearch.sbr"
	-@erase "$(INTDIR)\main.obj"
	-@erase "$(INTDIR)\main.sbr"
	-@erase "$(INTDIR)\notify.obj"
	-@erase "$(INTDIR)\notify.sbr"
	-@erase "$(INTDIR)\ntservice.obj"
	-@erase "$(INTDIR)\ntservice.sbr"
	-@erase "$(INTDIR)\os.obj"
	-@erase "$(INTDIR)\os.sbr"
	-@erase "$(INTDIR)\query.obj"
	-@erase "$(INTDIR)\query.sbr"
	-@erase "$(INTDIR)\server.obj"
	-@erase "$(INTDIR)\server.sbr"
	-@erase "$(INTDIR)\sortlist.obj"
	-@erase "$(INTDIR)\sortlist.sbr"
	-@erase "$(INTDIR)\tkeyconf.obj"
	-@erase "$(INTDIR)\tkeyconf.sbr"
	-@erase "$(INTDIR)\tsigconf.obj"
	-@erase "$(INTDIR)\tsigconf.sbr"
	-@erase "$(INTDIR)\update.obj"
	-@erase "$(INTDIR)\update.sbr"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(INTDIR)\xfrout.obj"
	-@erase "$(INTDIR)\xfrout.sbr"
	-@erase "$(INTDIR)\zoneconf.obj"
	-@erase "$(INTDIR)\zoneconf.sbr"
	-@erase "$(OUTDIR)\named.bsc"
	-@erase "$(OUTDIR)\named.map"
	-@erase "$(OUTDIR)\named.pdb"
	-@erase "..\..\..\Build\Debug\named.exe"
	-@erase "..\..\..\Build\Debug\named.ilk"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MDd /W3 /Gm /GX /ZI /Od /I "./" /I "../../../" /I "../win32/include" /I "../include" /I "../../../lib/isc/win32" /I "../../../lib/isc/win32/include" /I "../../../lib/isc/include" /I "../../../lib/dns/win32/include" /I "../../../lib/dns/include" /I "../../../lib/dns/sec/dst/include" /I "../../../lib/isccc/include" /I "../../../lib/lwres/win32/include" /I "../../../lib/lwres/include" /I "../../../lib/isccfg/include" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /D "i386" /FR"$(INTDIR)\\" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

RSC=rc.exe
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\named.bsc" 
BSC32_SBRS= \
	"$(INTDIR)\aclconf.sbr" \
	"$(INTDIR)\client.sbr" \
	"$(INTDIR)\config.sbr" \
	"$(INTDIR)\control.sbr" \
	"$(INTDIR)\controlconf.sbr" \
	"$(INTDIR)\interfacemgr.sbr" \
	"$(INTDIR)\listenlist.sbr" \
	"$(INTDIR)\log.sbr" \
	"$(INTDIR)\logconf.sbr" \
	"$(INTDIR)\lwaddr.sbr" \
	"$(INTDIR)\lwdclient.sbr" \
	"$(INTDIR)\lwderror.sbr" \
	"$(INTDIR)\lwdgabn.sbr" \
	"$(INTDIR)\lwdgnba.sbr" \
	"$(INTDIR)\lwdgrbn.sbr" \
	"$(INTDIR)\lwdnoop.sbr" \
	"$(INTDIR)\lwresd.sbr" \
	"$(INTDIR)\lwsearch.sbr" \
	"$(INTDIR)\main.sbr" \
	"$(INTDIR)\notify.sbr" \
	"$(INTDIR)\ntservice.sbr" \
	"$(INTDIR)\os.sbr" \
	"$(INTDIR)\query.sbr" \
	"$(INTDIR)\server.sbr" \
	"$(INTDIR)\sortlist.sbr" \
	"$(INTDIR)\tkeyconf.sbr" \
	"$(INTDIR)\tsigconf.sbr" \
	"$(INTDIR)\update.sbr" \
	"$(INTDIR)\xfrout.sbr" \
	"$(INTDIR)\zoneconf.sbr"

"$(OUTDIR)\named.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
LINK32_FLAGS=user32.lib advapi32.lib kernel32.lib ws2_32.lib ../../../lib/isc/win32/Debug/libisc.lib ../../../lib/dns/win32/Debug/libdns.lib ../../../lib/isccc/win32/Debug/libisccc.lib ../../../lib/lwres/win32/Debug/liblwres.lib ../../../lib/isccfg/win32/Debug/libisccfg.lib /nologo /subsystem:console /incremental:yes /pdb:"$(OUTDIR)\named.pdb" /map:"$(INTDIR)\named.map" /debug /machine:I386 /out:"../../../Build/Debug/named.exe" /pdbtype:sept 
LINK32_OBJS= \
	"$(INTDIR)\aclconf.obj" \
	"$(INTDIR)\client.obj" \
	"$(INTDIR)\config.obj" \
	"$(INTDIR)\control.obj" \
	"$(INTDIR)\controlconf.obj" \
	"$(INTDIR)\interfacemgr.obj" \
	"$(INTDIR)\listenlist.obj" \
	"$(INTDIR)\log.obj" \
	"$(INTDIR)\logconf.obj" \
	"$(INTDIR)\lwaddr.obj" \
	"$(INTDIR)\lwdclient.obj" \
	"$(INTDIR)\lwderror.obj" \
	"$(INTDIR)\lwdgabn.obj" \
	"$(INTDIR)\lwdgnba.obj" \
	"$(INTDIR)\lwdgrbn.obj" \
	"$(INTDIR)\lwdnoop.obj" \
	"$(INTDIR)\lwresd.obj" \
	"$(INTDIR)\lwsearch.obj" \
	"$(INTDIR)\main.obj" \
	"$(INTDIR)\notify.obj" \
	"$(INTDIR)\ntservice.obj" \
	"$(INTDIR)\os.obj" \
	"$(INTDIR)\query.obj" \
	"$(INTDIR)\server.obj" \
	"$(INTDIR)\sortlist.obj" \
	"$(INTDIR)\tkeyconf.obj" \
	"$(INTDIR)\tsigconf.obj" \
	"$(INTDIR)\update.obj" \
	"$(INTDIR)\xfrout.obj" \
	"$(INTDIR)\zoneconf.obj"

"..\..\..\Build\Debug\named.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("named.dep")
!INCLUDE "named.dep"
!ELSE 
!MESSAGE Warning: cannot find "named.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "named - Win32 Release" || "$(CFG)" == "named - Win32 Debug"
SOURCE=..\aclconf.c

!IF  "$(CFG)" == "named - Win32 Release"


"$(INTDIR)\aclconf.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "named - Win32 Debug"


"$(INTDIR)\aclconf.obj"	"$(INTDIR)\aclconf.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\client.c

!IF  "$(CFG)" == "named - Win32 Release"


"$(INTDIR)\client.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "named - Win32 Debug"


"$(INTDIR)\client.obj"	"$(INTDIR)\client.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\config.c

!IF  "$(CFG)" == "named - Win32 Release"


"$(INTDIR)\config.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "named - Win32 Debug"


"$(INTDIR)\config.obj"	"$(INTDIR)\config.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\control.c

!IF  "$(CFG)" == "named - Win32 Release"


"$(INTDIR)\control.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "named - Win32 Debug"


"$(INTDIR)\control.obj"	"$(INTDIR)\control.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\controlconf.c

!IF  "$(CFG)" == "named - Win32 Release"


"$(INTDIR)\controlconf.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "named - Win32 Debug"


"$(INTDIR)\controlconf.obj"	"$(INTDIR)\controlconf.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\interfacemgr.c

!IF  "$(CFG)" == "named - Win32 Release"


"$(INTDIR)\interfacemgr.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "named - Win32 Debug"


"$(INTDIR)\interfacemgr.obj"	"$(INTDIR)\interfacemgr.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\listenlist.c

!IF  "$(CFG)" == "named - Win32 Release"


"$(INTDIR)\listenlist.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "named - Win32 Debug"


"$(INTDIR)\listenlist.obj"	"$(INTDIR)\listenlist.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\log.c

!IF  "$(CFG)" == "named - Win32 Release"


"$(INTDIR)\log.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "named - Win32 Debug"


"$(INTDIR)\log.obj"	"$(INTDIR)\log.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\logconf.c

!IF  "$(CFG)" == "named - Win32 Release"


"$(INTDIR)\logconf.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "named - Win32 Debug"


"$(INTDIR)\logconf.obj"	"$(INTDIR)\logconf.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\lwaddr.c

!IF  "$(CFG)" == "named - Win32 Release"


"$(INTDIR)\lwaddr.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "named - Win32 Debug"


"$(INTDIR)\lwaddr.obj"	"$(INTDIR)\lwaddr.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\lwdclient.c

!IF  "$(CFG)" == "named - Win32 Release"


"$(INTDIR)\lwdclient.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "named - Win32 Debug"


"$(INTDIR)\lwdclient.obj"	"$(INTDIR)\lwdclient.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\lwderror.c

!IF  "$(CFG)" == "named - Win32 Release"


"$(INTDIR)\lwderror.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "named - Win32 Debug"


"$(INTDIR)\lwderror.obj"	"$(INTDIR)\lwderror.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\lwdgabn.c

!IF  "$(CFG)" == "named - Win32 Release"


"$(INTDIR)\lwdgabn.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "named - Win32 Debug"


"$(INTDIR)\lwdgabn.obj"	"$(INTDIR)\lwdgabn.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\lwdgnba.c

!IF  "$(CFG)" == "named - Win32 Release"


"$(INTDIR)\lwdgnba.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "named - Win32 Debug"


"$(INTDIR)\lwdgnba.obj"	"$(INTDIR)\lwdgnba.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\lwdgrbn.c

!IF  "$(CFG)" == "named - Win32 Release"


"$(INTDIR)\lwdgrbn.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "named - Win32 Debug"


"$(INTDIR)\lwdgrbn.obj"	"$(INTDIR)\lwdgrbn.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\lwdnoop.c

!IF  "$(CFG)" == "named - Win32 Release"


"$(INTDIR)\lwdnoop.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "named - Win32 Debug"


"$(INTDIR)\lwdnoop.obj"	"$(INTDIR)\lwdnoop.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\lwresd.c

!IF  "$(CFG)" == "named - Win32 Release"


"$(INTDIR)\lwresd.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "named - Win32 Debug"


"$(INTDIR)\lwresd.obj"	"$(INTDIR)\lwresd.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\lwsearch.c

!IF  "$(CFG)" == "named - Win32 Release"


"$(INTDIR)\lwsearch.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "named - Win32 Debug"


"$(INTDIR)\lwsearch.obj"	"$(INTDIR)\lwsearch.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\main.c

!IF  "$(CFG)" == "named - Win32 Release"


"$(INTDIR)\main.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "named - Win32 Debug"


"$(INTDIR)\main.obj"	"$(INTDIR)\main.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\notify.c

!IF  "$(CFG)" == "named - Win32 Release"


"$(INTDIR)\notify.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "named - Win32 Debug"


"$(INTDIR)\notify.obj"	"$(INTDIR)\notify.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=.\ntservice.c

!IF  "$(CFG)" == "named - Win32 Release"


"$(INTDIR)\ntservice.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "named - Win32 Debug"


"$(INTDIR)\ntservice.obj"	"$(INTDIR)\ntservice.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\os.c

!IF  "$(CFG)" == "named - Win32 Release"


"$(INTDIR)\os.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "named - Win32 Debug"


"$(INTDIR)\os.obj"	"$(INTDIR)\os.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=..\query.c

!IF  "$(CFG)" == "named - Win32 Release"


"$(INTDIR)\query.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "named - Win32 Debug"


"$(INTDIR)\query.obj"	"$(INTDIR)\query.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\server.c

!IF  "$(CFG)" == "named - Win32 Release"


"$(INTDIR)\server.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "named - Win32 Debug"


"$(INTDIR)\server.obj"	"$(INTDIR)\server.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\sortlist.c

!IF  "$(CFG)" == "named - Win32 Release"


"$(INTDIR)\sortlist.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "named - Win32 Debug"


"$(INTDIR)\sortlist.obj"	"$(INTDIR)\sortlist.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\tkeyconf.c

!IF  "$(CFG)" == "named - Win32 Release"


"$(INTDIR)\tkeyconf.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "named - Win32 Debug"


"$(INTDIR)\tkeyconf.obj"	"$(INTDIR)\tkeyconf.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\tsigconf.c

!IF  "$(CFG)" == "named - Win32 Release"


"$(INTDIR)\tsigconf.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "named - Win32 Debug"


"$(INTDIR)\tsigconf.obj"	"$(INTDIR)\tsigconf.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\update.c

!IF  "$(CFG)" == "named - Win32 Release"


"$(INTDIR)\update.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "named - Win32 Debug"


"$(INTDIR)\update.obj"	"$(INTDIR)\update.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\xfrout.c

!IF  "$(CFG)" == "named - Win32 Release"


"$(INTDIR)\xfrout.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "named - Win32 Debug"


"$(INTDIR)\xfrout.obj"	"$(INTDIR)\xfrout.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\zoneconf.c

!IF  "$(CFG)" == "named - Win32 Release"


"$(INTDIR)\zoneconf.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "named - Win32 Debug"


"$(INTDIR)\zoneconf.obj"	"$(INTDIR)\zoneconf.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 


!ENDIF 

