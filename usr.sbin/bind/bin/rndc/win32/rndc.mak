# Microsoft Developer Studio Generated NMAKE File, Based on rndc.dsp
!IF "$(CFG)" == ""
CFG=rndc - Win32 Debug
!MESSAGE No configuration specified. Defaulting to rndc - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "rndc - Win32 Release" && "$(CFG)" != "rndc - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "rndc.mak" CFG="rndc - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "rndc - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "rndc - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "rndc - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release

ALL : "..\..\..\Build\Release\rndc.exe"


CLEAN :
	-@erase "$(INTDIR)\rndc.obj"
	-@erase "$(INTDIR)\util.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "..\..\..\Build\Release\rndc.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MT /W3 /GX /O2 /I "./" /I "../../../" /I "../../../lib/isc/win32" /I "../../../lib/isc/win32/include" /I "../../../lib/isc/include" /I "../../../lib/isccc/include" /I "../../../lib/isccfg/include" /D "WIN32" /D "NDEBUG" /D "__STDC__" /D "_CONSOLE" /D "_MBCS" /Fp"$(INTDIR)\rndc.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\rndc.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=user32.lib advapi32.lib ws2_32.lib ../../../lib/isc/win32/Release/libisc.lib ../../../lib/dns/win32/Release/libdns.lib ../../../lib/isccfg/win32/Release/libisccfg.lib ../../../lib/isccc/win32/Release/libisccc.lib /nologo /subsystem:console /incremental:no /pdb:"$(OUTDIR)\rndc.pdb" /machine:I386 /out:"../../../Build/Release/rndc.exe" 
LINK32_OBJS= \
	"$(INTDIR)\rndc.obj" \
	"$(INTDIR)\util.obj"

"..\..\..\Build\Release\rndc.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "rndc - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

ALL : "..\..\..\Build\Debug\rndc.exe" "$(OUTDIR)\rndc.bsc"


CLEAN :
	-@erase "$(INTDIR)\rndc.obj"
	-@erase "$(INTDIR)\rndc.sbr"
	-@erase "$(INTDIR)\util.obj"
	-@erase "$(INTDIR)\util.sbr"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(OUTDIR)\rndc.bsc"
	-@erase "$(OUTDIR)\rndc.pdb"
	-@erase "..\..\..\Build\Debug\rndc.exe"
	-@erase "..\..\..\Build\Debug\rndc.ilk"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MTd /W3 /Gm /GX /ZI /Od /I "./" /I "../../../" /I "../../../lib/isc/win32" /I "../../../lib/isc/win32/include" /I "../../../lib/isc/include" /I "../../../lib/isccc/include" /I "../../../lib/isccfg/include" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /FR"$(INTDIR)\\" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\rndc.bsc" 
BSC32_SBRS= \
	"$(INTDIR)\rndc.sbr" \
	"$(INTDIR)\util.sbr"

"$(OUTDIR)\rndc.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
LINK32_FLAGS=user32.lib advapi32.lib ws2_32.lib ../../../lib/isc/win32/Debug/libisc.lib ../../../lib/dns/win32/Debug/libdns.lib ../../../lib/isccfg/win32/Debug/libisccfg.lib ../../../lib/isccc/win32/Debug/libisccc.lib /nologo /subsystem:console /incremental:yes /pdb:"$(OUTDIR)\rndc.pdb" /debug /machine:I386 /out:"../../../Build/Debug/rndc.exe" /pdbtype:sept 
LINK32_OBJS= \
	"$(INTDIR)\rndc.obj" \
	"$(INTDIR)\util.obj"

"..\..\..\Build\Debug\rndc.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("rndc.dep")
!INCLUDE "rndc.dep"
!ELSE 
!MESSAGE Warning: cannot find "rndc.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "rndc - Win32 Release" || "$(CFG)" == "rndc - Win32 Debug"
SOURCE=..\rndc.c

!IF  "$(CFG)" == "rndc - Win32 Release"


"$(INTDIR)\rndc.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "rndc - Win32 Debug"


"$(INTDIR)\rndc.obj"	"$(INTDIR)\rndc.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\util.c

!IF  "$(CFG)" == "rndc - Win32 Release"


"$(INTDIR)\util.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "rndc - Win32 Debug"


"$(INTDIR)\util.obj"	"$(INTDIR)\util.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 


!ENDIF 

