# Microsoft Developer Studio Generated NMAKE File, Based on nsupdate.dsp
!IF "$(CFG)" == ""
CFG=nsupdate - Win32 Debug
!MESSAGE No configuration specified. Defaulting to nsupdate - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "nsupdate - Win32 Release" && "$(CFG)" != "nsupdate - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "nsupdate.mak" CFG="nsupdate - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "nsupdate - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "nsupdate - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "nsupdate - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release

ALL : "..\..\..\Build\Release\nsupdate.exe"


CLEAN :
	-@erase "$(INTDIR)\nsupdate.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "..\..\..\Build\Release\nsupdate.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MT /W3 /GX /O2 /I "./" /I "../include" /I "../../../" /I "../../../lib/isc/win32" /I "../../../lib/isc/win32/include" /I "../../../lib/isc/include" /I "../../../lib/lwres/win32/include" /I "../../../lib/lwres/include" /I "../../../lib/lwres/win32/include/lwres" /I "../../../lib/dns/include" /I "../../../lib/dns/sec/dst/include" /D "WIN32" /D "__STDC__" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /Fp"$(INTDIR)\nsupdate.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\nsupdate.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=../../../lib/isc/win32/Release/libisc.lib ../../../lib/dns/win32/Release/libdns.lib ../../../lib/lwres/win32/Release/liblwres.lib user32.lib advapi32.lib ws2_32.lib /nologo /subsystem:console /incremental:no /pdb:"$(OUTDIR)\nsupdate.pdb" /machine:I386 /out:"../../../Build/Release/nsupdate.exe" 
LINK32_OBJS= \
	"$(INTDIR)\nsupdate.obj"

"..\..\..\Build\Release\nsupdate.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "nsupdate - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

ALL : "..\..\..\Build\Debug\nsupdate.exe" "$(OUTDIR)\nsupdate.bsc"


CLEAN :
	-@erase "$(INTDIR)\nsupdate.obj"
	-@erase "$(INTDIR)\nsupdate.sbr"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(OUTDIR)\nsupdate.bsc"
	-@erase "$(OUTDIR)\nsupdate.pdb"
	-@erase "..\..\..\Build\Debug\nsupdate.exe"
	-@erase "..\..\..\Build\Debug\nsupdate.ilk"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MTd /W3 /Gm /GX /ZI /Od /I "./" /I "../include" /I "../../../" /I "../../../lib/isc/win32" /I "../../../lib/isc/win32/include" /I "../../../lib/isc/include" /I "../../../lib/lwres/win32/include" /I "../../../lib/lwres/include" /I "../../../lib/lwres/win32/include/lwres" /I "../../../lib/dns/include" /I "../../../lib/dns/sec/dst/include" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /FR"$(INTDIR)\\" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\nsupdate.bsc" 
BSC32_SBRS= \
	"$(INTDIR)\nsupdate.sbr"

"$(OUTDIR)\nsupdate.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
LINK32_FLAGS=../../../lib/isc/win32/Debug/libisc.lib ../../../lib/dns/win32/Debug/libdns.lib ../../../lib/lwres/win32/Debug/liblwres.lib user32.lib advapi32.lib ws2_32.lib /nologo /subsystem:console /incremental:yes /pdb:"$(OUTDIR)\nsupdate.pdb" /debug /machine:I386 /out:"../../../Build/Debug/nsupdate.exe" /pdbtype:sept 
LINK32_OBJS= \
	"$(INTDIR)\nsupdate.obj"

"..\..\..\Build\Debug\nsupdate.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("nsupdate.dep")
!INCLUDE "nsupdate.dep"
!ELSE 
!MESSAGE Warning: cannot find "nsupdate.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "nsupdate - Win32 Release" || "$(CFG)" == "nsupdate - Win32 Debug"
SOURCE=..\nsupdate.c

!IF  "$(CFG)" == "nsupdate - Win32 Release"


"$(INTDIR)\nsupdate.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "nsupdate - Win32 Debug"


"$(INTDIR)\nsupdate.obj"	"$(INTDIR)\nsupdate.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 


!ENDIF 

