# Microsoft Developer Studio Generated NMAKE File, Based on gen.dsp
!IF "$(CFG)" == ""
CFG=gen - Win32 Debug
!MESSAGE No configuration specified. Defaulting to gen - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "gen - Win32 Release" && "$(CFG)" != "gen - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "gen.mak" CFG="gen - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "gen - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "gen - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "gen - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release

ALL : "..\gen.exe"


CLEAN :
	-@erase "$(INTDIR)\gen.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "..\gen.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /ML /W3 /GX /O2 /I "./" /I "../../../" /I "../../../lib/isc/win32" /I "../../../lib/isc/win32/include" /I "../../../lib/isc/include" /D "NDEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "LIBISC_EXPORTS" /Fp"$(INTDIR)\gen.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\gen.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=user32.lib advapi32.lib /nologo /subsystem:console /incremental:no /pdb:"$(OUTDIR)\gen.pdb" /machine:I386 /out:"../gen.exe" 
LINK32_OBJS= \
	"$(INTDIR)\gen.obj"

"..\gen.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "gen - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

ALL : "..\gen.exe" "$(OUTDIR)\gen.bsc"


CLEAN :
	-@erase "$(INTDIR)\gen.obj"
	-@erase "$(INTDIR)\gen.sbr"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(OUTDIR)\gen.bsc"
	-@erase "$(OUTDIR)\gen.pdb"
	-@erase "..\gen.exe"
	-@erase "..\gen.ilk"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MLd /W3 /Gm /GX /ZI /Od /I "./" /I "../../../" /I "../../../lib/isc/win32" /I "../../../lib/isc/win32/include" /I "../../../lib/isc/include" /D "_DEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "LIBISC_EXPORTS" /FR"$(INTDIR)\\" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\gen.bsc" 
BSC32_SBRS= \
	"$(INTDIR)\gen.sbr"

"$(OUTDIR)\gen.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
LINK32_FLAGS=user32.lib advapi32.lib /nologo /subsystem:console /incremental:yes /pdb:"$(OUTDIR)\gen.pdb" /debug /machine:I386 /out:"../gen.exe" /pdbtype:sept 
LINK32_OBJS= \
	"$(INTDIR)\gen.obj"

"..\gen.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 

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


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("gen.dep")
!INCLUDE "gen.dep"
!ELSE 
!MESSAGE Warning: cannot find "gen.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "gen - Win32 Release" || "$(CFG)" == "gen - Win32 Debug"
SOURCE=..\gen.c

!IF  "$(CFG)" == "gen - Win32 Release"


"$(INTDIR)\gen.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "gen - Win32 Debug"


"$(INTDIR)\gen.obj"	"$(INTDIR)\gen.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 


!ENDIF 

