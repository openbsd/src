# Microsoft Developer Studio Generated NMAKE File, Based on namedcheckzone.dsp
!IF "$(CFG)" == ""
CFG=namedcheckzone - Win32 Debug
!MESSAGE No configuration specified. Defaulting to namedcheckzone - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "namedcheckzone - Win32 Release" && "$(CFG)" != "namedcheckzone - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "namedcheckzone.mak" CFG="namedcheckzone - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "namedcheckzone - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "namedcheckzone - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "namedcheckzone - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

ALL : "..\..\..\Build\Release\named-checkzone.exe" "$(OUTDIR)\namedcheckzone.bsc"


CLEAN :
	-@erase "$(INTDIR)\check-tool.obj"
	-@erase "$(INTDIR)\check-tool.sbr"
	-@erase "$(INTDIR)\named-checkzone.obj"
	-@erase "$(INTDIR)\named-checkzone.sbr"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\namedcheckzone.bsc"
	-@erase "..\..\..\Build\Release\named-checkzone.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MT /W3 /GX /O2 /I "./" /I "../../../" /I "../../../lib/isc/win32" /I "../../../lib/isc/win32/include" /I "../../../lib/isc/include" /I "../../../lib/dns/include" /D "NDEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "__STDC__" /FR"$(INTDIR)\\" /Fp"$(INTDIR)\namedcheckzone.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\namedcheckzone.bsc" 
BSC32_SBRS= \
	"$(INTDIR)\check-tool.sbr" \
	"$(INTDIR)\named-checkzone.sbr"

"$(OUTDIR)\namedcheckzone.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
LINK32_FLAGS=user32.lib advapi32.lib ws2_32.lib ../../../lib/isc/win32/Release/libisc.lib ../../../lib/dns/win32/Release/libdns.lib /nologo /subsystem:console /incremental:no /pdb:"$(OUTDIR)\named-checkzone.pdb" /machine:I386 /out:"../../../Build/Release/named-checkzone.exe" 
LINK32_OBJS= \
	"$(INTDIR)\check-tool.obj" \
	"$(INTDIR)\named-checkzone.obj"

"..\..\..\Build\Release\named-checkzone.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "namedcheckzone - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

ALL : "..\..\..\Build\Debug\named-checkzone.exe" "$(OUTDIR)\namedcheckzone.bsc"


CLEAN :
	-@erase "$(INTDIR)\check-tool.obj"
	-@erase "$(INTDIR)\check-tool.sbr"
	-@erase "$(INTDIR)\named-checkzone.obj"
	-@erase "$(INTDIR)\named-checkzone.sbr"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(OUTDIR)\named-checkzone.pdb"
	-@erase "$(OUTDIR)\namedcheckzone.bsc"
	-@erase "..\..\..\Build\Debug\named-checkzone.exe"
	-@erase "..\..\..\Build\Debug\named-checkzone.ilk"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MTd /W3 /Gm /GX /ZI /Od /I "./" /I "../../../" /I "../../../lib/isc/win32" /I "../../../lib/isc/win32/include" /I "../../../lib/isc/include" /I "../../../lib/dns/include" /D "_DEBUG" /D "__STDC__" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /FR"$(INTDIR)\\" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\namedcheckzone.bsc" 
BSC32_SBRS= \
	"$(INTDIR)\check-tool.sbr" \
	"$(INTDIR)\named-checkzone.sbr"

"$(OUTDIR)\namedcheckzone.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
LINK32_FLAGS=user32.lib advapi32.lib ws2_32.lib ../../../lib/isc/win32/Debug/libisc.lib ../../../lib/dns/win32/Debug/libdns.lib /nologo /subsystem:console /incremental:yes /pdb:"$(OUTDIR)\named-checkzone.pdb" /debug /machine:I386 /out:"../../../Build/Debug/named-checkzone.exe" /pdbtype:sept 
LINK32_OBJS= \
	"$(INTDIR)\check-tool.obj" \
	"$(INTDIR)\named-checkzone.obj"

"..\..\..\Build\Debug\named-checkzone.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("namedcheckzone.dep")
!INCLUDE "namedcheckzone.dep"
!ELSE 
!MESSAGE Warning: cannot find "namedcheckzone.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "namedcheckzone - Win32 Release" || "$(CFG)" == "namedcheckzone - Win32 Debug"
SOURCE="..\check-tool.c"

"$(INTDIR)\check-tool.obj"	"$(INTDIR)\check-tool.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\named-checkzone.c"

"$(INTDIR)\named-checkzone.obj"	"$(INTDIR)\named-checkzone.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)



!ENDIF 

