# Microsoft Developer Studio Generated NMAKE File, Based on bindevt.dsp
!IF "$(CFG)" == ""
CFG=bindevt - Win32 Debug
!MESSAGE No configuration specified. Defaulting to bindevt - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "bindevt - Win32 Release" && "$(CFG)" != "bindevt - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "bindevt.mak" CFG="bindevt - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "bindevt - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "bindevt - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "bindevt - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release

ALL : "..\..\..\Build\Release\bindevt.dll"


CLEAN :
	-@erase "$(INTDIR)\bindevt.obj"
	-@erase "$(INTDIR)\bindevt.res"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\bindevt.exp"
	-@erase "..\..\..\Build\Release\bindevt.dll"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MT /W3 /GX /O2 /I "..\include" /I "..\..\..\include" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "BINDEVT_EXPORTS" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
MTL_PROJ=/nologo /D "NDEBUG" /mktyplib203 /win32 
RSC_PROJ=/l 0x409 /fo"$(INTDIR)\bindevt.res" /d "NDEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\bindevt.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /pdb:none /machine:I386 /out:"..\..\..\Build\Release\bindevt.dll" /implib:"$(OUTDIR)\bindevt.lib" 
LINK32_OBJS= \
	"$(INTDIR)\bindevt.obj" \
	"$(INTDIR)\bindevt.res"

"..\..\..\Build\Release\bindevt.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "bindevt - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

ALL : "..\..\..\Build\Debug\bindevt.dll" "$(OUTDIR)\bindevt.bsc"


CLEAN :
	-@erase "$(INTDIR)\bindevt.obj"
	-@erase "$(INTDIR)\bindevt.res"
	-@erase "$(INTDIR)\bindevt.sbr"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(OUTDIR)\bindevt.bsc"
	-@erase "$(OUTDIR)\bindevt.exp"
	-@erase "..\..\..\Build\Debug\bindevt.dll"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MTd /W3 /Gm /GX /Zi /Od /I "..\include" /I "..\..\..\include" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "BINDEVT_EXPORTS" /FR"$(INTDIR)\\" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 
MTL_PROJ=/nologo /D "_DEBUG" /mktyplib203 /win32 
RSC_PROJ=/l 0x409 /fo"$(INTDIR)\bindevt.res" /d "_DEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\bindevt.bsc" 
BSC32_SBRS= \
	"$(INTDIR)\bindevt.sbr"

"$(OUTDIR)\bindevt.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /pdb:none /debug /machine:I386 /out:"..\..\..\Build\Debug\bindevt.dll" /implib:"$(OUTDIR)\bindevt.lib" 
LINK32_OBJS= \
	"$(INTDIR)\bindevt.obj" \
	"$(INTDIR)\bindevt.res"

"..\..\..\Build\Debug\bindevt.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
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
!IF EXISTS("bindevt.dep")
!INCLUDE "bindevt.dep"
!ELSE 
!MESSAGE Warning: cannot find "bindevt.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "bindevt - Win32 Release" || "$(CFG)" == "bindevt - Win32 Debug"
SOURCE=.\bindevt.c

!IF  "$(CFG)" == "bindevt - Win32 Release"


"$(INTDIR)\bindevt.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "bindevt - Win32 Debug"


"$(INTDIR)\bindevt.obj"	"$(INTDIR)\bindevt.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\bindevt.mc

!IF  "$(CFG)" == "bindevt - Win32 Release"

TargetName=bindevt
InputPath=.\bindevt.mc
InputName=bindevt

".\bindevt.rc" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	<<tempfile.bat 
	@echo off 
	mc $(InputName).mc
<< 
	

!ELSEIF  "$(CFG)" == "bindevt - Win32 Debug"

TargetName=bindevt
InputPath=.\bindevt.mc
InputName=bindevt

".\bindevt.rc" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	<<tempfile.bat 
	@echo off 
	mc $(InputName).mc
<< 
	

!ENDIF 

SOURCE=.\bindevt.rc

"$(INTDIR)\bindevt.res" : $(SOURCE) "$(INTDIR)"
	$(RSC) $(RSC_PROJ) $(SOURCE)



!ENDIF 

