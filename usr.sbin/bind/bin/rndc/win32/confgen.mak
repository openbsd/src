# Microsoft Developer Studio Generated NMAKE File, Based on confgen.dsp
!IF "$(CFG)" == ""
CFG=rndcconfgen - Win32 Debug
!MESSAGE No configuration specified. Defaulting to rndcconfgen - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "rndcconfgen - Win32 Release" && "$(CFG)" != "rndcconfgen - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "confgen.mak" CFG="rndcconfgen - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "rndcconfgen - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "rndcconfgen - Win32 Debug" (based on "Win32 (x86) Console Application")
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

!IF  "$(CFG)" == "rndcconfgen - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release

ALL : "..\..\..\Build\Release\rndc-confgen.exe"


CLEAN :
	-@erase "$(INTDIR)\os.obj"
	-@erase "$(INTDIR)\rndc-confgen.obj"
	-@erase "$(INTDIR)\util.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "..\..\..\Build\Release\rndc-confgen.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MT /W3 /GX /O2 /I "./" /I "../../../" /I "../include" /I "../../../lib/isc/win32" /I "../../../lib/isc/win32/include" /I "../../../lib/isc/include" /I "../../../lib/dns/win32/include" /I "../../../lib/dns/include" /I "../../../lib/dns/sec/dst/include" /I "../../../lib/isccc/include" /I "../../../lib/isccfg/include" /D "WIN32" /D "NDEBUG" /D "__STDC__" /D "_CONSOLE" /D "_MBCS" /Fp"$(INTDIR)\confgen.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\confgen.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=user32.lib advapi32.lib ws2_32.lib ../../../lib/isc/win32/Release/libisc.lib ../../../lib/dns/win32/Release/libdns.lib ../../../lib/isccfg/win32/Release/libisccfg.lib ../../../lib/isccc/win32/Release/libisccc.lib /nologo /subsystem:console /incremental:no /pdb:"$(OUTDIR)\rndc-confgen.pdb" /machine:I386 /out:"../../../Build/Release/rndc-confgen.exe" 
LINK32_OBJS= \
	"$(INTDIR)\os.obj" \
	"$(INTDIR)\rndc-confgen.obj" \
	"$(INTDIR)\util.obj"

"..\..\..\Build\Release\rndc-confgen.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "rndcconfgen - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

ALL : "..\..\..\Build\Debug\rndc-confgen.exe" "$(OUTDIR)\confgen.bsc"


CLEAN :
	-@erase "$(INTDIR)\os.obj"
	-@erase "$(INTDIR)\os.sbr"
	-@erase "$(INTDIR)\rndc-confgen.obj"
	-@erase "$(INTDIR)\rndc-confgen.sbr"
	-@erase "$(INTDIR)\util.obj"
	-@erase "$(INTDIR)\util.sbr"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(OUTDIR)\confgen.bsc"
	-@erase "$(OUTDIR)\rndc-confgen.pdb"
	-@erase "..\..\..\Build\Debug\rndc-confgen.exe"
	-@erase "..\..\..\Build\Debug\rndc-confgen.ilk"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MTd /W3 /Gm /GX /ZI /Od /I "./" /I "../../../" /I "../include" /I "../../../lib/isc/win32" /I "../../../lib/isc/win32/include" /I "../../../lib/isc/include" /I "../../../lib/dns/win32/include" /I "../../../lib/dns/include" /I "../../../lib/dns/sec/dst/include" /I "../../../lib/isccc/include" /I "../../../lib/isccfg/include" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /FR"$(INTDIR)\\" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\confgen.bsc" 
BSC32_SBRS= \
	"$(INTDIR)\os.sbr" \
	"$(INTDIR)\rndc-confgen.sbr" \
	"$(INTDIR)\util.sbr"

"$(OUTDIR)\confgen.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
LINK32_FLAGS=user32.lib advapi32.lib ws2_32.lib ../../../lib/isc/win32/Debug/libisc.lib ../../../lib/dns/win32/Debug/libdns.lib ../../../lib/isccfg/win32/Debug/libisccfg.lib ../../../lib/isccc/win32/Debug/libisccc.lib /nologo /subsystem:console /incremental:yes /pdb:"$(OUTDIR)\rndc-confgen.pdb" /debug /machine:I386 /out:"../../../Build/Debug/rndc-confgen.exe" /pdbtype:sept 
LINK32_OBJS= \
	"$(INTDIR)\os.obj" \
	"$(INTDIR)\rndc-confgen.obj" \
	"$(INTDIR)\util.obj"

"..\..\..\Build\Debug\rndc-confgen.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
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
!IF EXISTS("confgen.dep")
!INCLUDE "confgen.dep"
!ELSE 
!MESSAGE Warning: cannot find "confgen.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "rndcconfgen - Win32 Release" || "$(CFG)" == "rndcconfgen - Win32 Debug"
SOURCE=.\os.c

!IF  "$(CFG)" == "rndcconfgen - Win32 Release"


"$(INTDIR)\os.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "rndcconfgen - Win32 Debug"


"$(INTDIR)\os.obj"	"$(INTDIR)\os.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE="..\rndc-confgen.c"

!IF  "$(CFG)" == "rndcconfgen - Win32 Release"


"$(INTDIR)\rndc-confgen.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "rndcconfgen - Win32 Debug"


"$(INTDIR)\rndc-confgen.obj"	"$(INTDIR)\rndc-confgen.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\util.c

!IF  "$(CFG)" == "rndcconfgen - Win32 Release"


"$(INTDIR)\util.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "rndcconfgen - Win32 Debug"


"$(INTDIR)\util.obj"	"$(INTDIR)\util.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 


!ENDIF 

