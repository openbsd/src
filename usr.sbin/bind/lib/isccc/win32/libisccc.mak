# Microsoft Developer Studio Generated NMAKE File, Based on libisccc.dsp
!IF "$(CFG)" == ""
CFG=libisccc - Win32 Release
!MESSAGE No configuration specified. Defaulting to libisccc - Win32 Release.
!ENDIF 

!IF "$(CFG)" != "libisccc - Win32 Release" && "$(CFG)" != "libisccc - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
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
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "libisccc - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release

ALL : "..\..\..\Build\Release\libisccc.dll"


CLEAN :
	-@erase "$(INTDIR)\alist.obj"
	-@erase "$(INTDIR)\base64.obj"
	-@erase "$(INTDIR)\cc.obj"
	-@erase "$(INTDIR)\ccmsg.obj"
	-@erase "$(INTDIR)\DLLMain.obj"
	-@erase "$(INTDIR)\lib.obj"
	-@erase "$(INTDIR)\result.obj"
	-@erase "$(INTDIR)\sexpr.obj"
	-@erase "$(INTDIR)\symtab.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\version.obj"
	-@erase "$(OUTDIR)\libisccc.exp"
	-@erase "$(OUTDIR)\libisccc.lib"
	-@erase "..\..\..\Build\Release\libisccc.dll"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MD /W3 /GX /O2 /I "./" /I "../../../" /I "include" /I "../include" /I "../../../lib/isc/win32" /I "../../../lib/isc/win32/include" /I "../../../lib/dns/win32/include" /I "../../../lib/dns/include" /I "../../../lib/isc/include" /I "../..../lib/dns/sec/openssl/include" /I "../../../lib/dns/sec/dst/include" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "__STDC__" /D "_MBCS" /D "_USRDLL" /D "USE_MD5" /D "OPENSSL" /D "DST_USE_PRIVATE_OPENSSL" /D "LIBISCCC_EXPORTS" /Fp"$(INTDIR)\libisccc.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

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

MTL=midl.exe
MTL_PROJ=/nologo /D "NDEBUG" /mktyplib203 /win32 
RSC=rc.exe
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\libisccc.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=user32.lib advapi32.lib ws2_32.lib ../../isc/win32/Release/libisc.lib /nologo /dll /incremental:no /pdb:"$(OUTDIR)\libisccc.pdb" /machine:I386 /def:".\libisccc.def" /out:"../../../Build/Release/libisccc.dll" /implib:"$(OUTDIR)\libisccc.lib" 
DEF_FILE= \
	".\libisccc.def"
LINK32_OBJS= \
	"$(INTDIR)\alist.obj" \
	"$(INTDIR)\base64.obj" \
	"$(INTDIR)\cc.obj" \
	"$(INTDIR)\ccmsg.obj" \
	"$(INTDIR)\DLLMain.obj" \
	"$(INTDIR)\lib.obj" \
	"$(INTDIR)\result.obj" \
	"$(INTDIR)\sexpr.obj" \
	"$(INTDIR)\symtab.obj" \
	"$(INTDIR)\version.obj"

"..\..\..\Build\Release\libisccc.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "libisccc - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

ALL : "..\..\..\Build\Debug\libisccc.dll" "$(OUTDIR)\libisccc.bsc"


CLEAN :
	-@erase "$(INTDIR)\alist.obj"
	-@erase "$(INTDIR)\alist.sbr"
	-@erase "$(INTDIR)\base64.obj"
	-@erase "$(INTDIR)\base64.sbr"
	-@erase "$(INTDIR)\cc.obj"
	-@erase "$(INTDIR)\cc.sbr"
	-@erase "$(INTDIR)\ccmsg.obj"
	-@erase "$(INTDIR)\ccmsg.sbr"
	-@erase "$(INTDIR)\DLLMain.obj"
	-@erase "$(INTDIR)\DLLMain.sbr"
	-@erase "$(INTDIR)\lib.obj"
	-@erase "$(INTDIR)\lib.sbr"
	-@erase "$(INTDIR)\result.obj"
	-@erase "$(INTDIR)\result.sbr"
	-@erase "$(INTDIR)\sexpr.obj"
	-@erase "$(INTDIR)\sexpr.sbr"
	-@erase "$(INTDIR)\symtab.obj"
	-@erase "$(INTDIR)\symtab.sbr"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(INTDIR)\version.obj"
	-@erase "$(INTDIR)\version.sbr"
	-@erase "$(OUTDIR)\libisccc.bsc"
	-@erase "$(OUTDIR)\libisccc.exp"
	-@erase "$(OUTDIR)\libisccc.lib"
	-@erase "$(OUTDIR)\libisccc.pdb"
	-@erase "..\..\..\Build\Debug\libisccc.dll"
	-@erase "..\..\..\Build\Debug\libisccc.ilk"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MDd /W3 /Gm /GX /ZI /Od /I "./" /I "../../../" /I "include" /I "../include" /I "../../../lib/isc/win32" /I "../../../lib/isc/win32/include" /I "../../../lib/dns/win32/include" /I "../../../lib/dns/include" /I "../../../lib/isc/include" /I "../..../lib/dns/sec/openssl/include" /I "../../../lib/dns/sec/dst/include" /D "_DEBUG" /D "WIN32" /D "__STDC__" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "USE_MD5" /D "OPENSSL" /D "DST_USE_PRIVATE_OPENSSL" /D "LIBISCCC_EXPORTS" /FR"$(INTDIR)\\" /Fp"$(INTDIR)\libisccc.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

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

MTL=midl.exe
MTL_PROJ=/nologo /D "_DEBUG" /mktyplib203 /win32 
RSC=rc.exe
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\libisccc.bsc" 
BSC32_SBRS= \
	"$(INTDIR)\alist.sbr" \
	"$(INTDIR)\base64.sbr" \
	"$(INTDIR)\cc.sbr" \
	"$(INTDIR)\ccmsg.sbr" \
	"$(INTDIR)\DLLMain.sbr" \
	"$(INTDIR)\lib.sbr" \
	"$(INTDIR)\result.sbr" \
	"$(INTDIR)\sexpr.sbr" \
	"$(INTDIR)\symtab.sbr" \
	"$(INTDIR)\version.sbr"

"$(OUTDIR)\libisccc.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
LINK32_FLAGS=user32.lib advapi32.lib ws2_32.lib ../../isc/win32/debug/libisc.lib /nologo /dll /incremental:yes /pdb:"$(OUTDIR)\libisccc.pdb" /debug /machine:I386 /def:".\libisccc.def" /out:"../../../Build/Debug/libisccc.dll" /implib:"$(OUTDIR)\libisccc.lib" /pdbtype:sept 
DEF_FILE= \
	".\libisccc.def"
LINK32_OBJS= \
	"$(INTDIR)\alist.obj" \
	"$(INTDIR)\base64.obj" \
	"$(INTDIR)\cc.obj" \
	"$(INTDIR)\ccmsg.obj" \
	"$(INTDIR)\DLLMain.obj" \
	"$(INTDIR)\lib.obj" \
	"$(INTDIR)\result.obj" \
	"$(INTDIR)\sexpr.obj" \
	"$(INTDIR)\symtab.obj" \
	"$(INTDIR)\version.obj"

"..\..\..\Build\Debug\libisccc.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("libisccc.dep")
!INCLUDE "libisccc.dep"
!ELSE 
!MESSAGE Warning: cannot find "libisccc.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "libisccc - Win32 Release" || "$(CFG)" == "libisccc - Win32 Debug"
SOURCE=..\alist.c

!IF  "$(CFG)" == "libisccc - Win32 Release"


"$(INTDIR)\alist.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "libisccc - Win32 Debug"


"$(INTDIR)\alist.obj"	"$(INTDIR)\alist.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\base64.c

!IF  "$(CFG)" == "libisccc - Win32 Release"


"$(INTDIR)\base64.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "libisccc - Win32 Debug"


"$(INTDIR)\base64.obj"	"$(INTDIR)\base64.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\cc.c

!IF  "$(CFG)" == "libisccc - Win32 Release"


"$(INTDIR)\cc.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "libisccc - Win32 Debug"


"$(INTDIR)\cc.obj"	"$(INTDIR)\cc.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\ccmsg.c

!IF  "$(CFG)" == "libisccc - Win32 Release"


"$(INTDIR)\ccmsg.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "libisccc - Win32 Debug"


"$(INTDIR)\ccmsg.obj"	"$(INTDIR)\ccmsg.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=.\DLLMain.c

!IF  "$(CFG)" == "libisccc - Win32 Release"


"$(INTDIR)\DLLMain.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "libisccc - Win32 Debug"


"$(INTDIR)\DLLMain.obj"	"$(INTDIR)\DLLMain.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=..\lib.c

!IF  "$(CFG)" == "libisccc - Win32 Release"


"$(INTDIR)\lib.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "libisccc - Win32 Debug"


"$(INTDIR)\lib.obj"	"$(INTDIR)\lib.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\result.c

!IF  "$(CFG)" == "libisccc - Win32 Release"


"$(INTDIR)\result.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "libisccc - Win32 Debug"


"$(INTDIR)\result.obj"	"$(INTDIR)\result.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\sexpr.c

!IF  "$(CFG)" == "libisccc - Win32 Release"


"$(INTDIR)\sexpr.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "libisccc - Win32 Debug"


"$(INTDIR)\sexpr.obj"	"$(INTDIR)\sexpr.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\symtab.c

!IF  "$(CFG)" == "libisccc - Win32 Release"


"$(INTDIR)\symtab.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "libisccc - Win32 Debug"


"$(INTDIR)\symtab.obj"	"$(INTDIR)\symtab.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=.\version.c

!IF  "$(CFG)" == "libisccc - Win32 Release"


"$(INTDIR)\version.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "libisccc - Win32 Debug"


"$(INTDIR)\version.obj"	"$(INTDIR)\version.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 


!ENDIF 

