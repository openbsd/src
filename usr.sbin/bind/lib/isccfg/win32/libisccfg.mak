# Microsoft Developer Studio Generated NMAKE File, Based on libisccfg.dsp
!IF "$(CFG)" == ""
CFG=libisccfg - Win32 Debug
!MESSAGE No configuration specified. Defaulting to libisccfg - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "libisccfg - Win32 Release" && "$(CFG)" != "libisccfg - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "libisccfg.mak" CFG="libisccfg - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "libisccfg - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "libisccfg - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
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

!IF  "$(CFG)" == "libisccfg - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release

!IF "$(RECURSE)" == "0" 

ALL : "..\..\..\Build\Release\libisccfg.dll"

!ELSE 

ALL : "libdns - Win32 Release" "libisc - Win32 Release" "..\..\..\Build\Release\libisccfg.dll"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"libisc - Win32 ReleaseCLEAN" "libdns - Win32 ReleaseCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\check.obj"
	-@erase "$(INTDIR)\DLLMain.obj"
	-@erase "$(INTDIR)\log.obj"
	-@erase "$(INTDIR)\parser.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\version.obj"
	-@erase "$(OUTDIR)\libisccfg.exp"
	-@erase "$(OUTDIR)\libisccfg.lib"
	-@erase "..\..\..\Build\Release\libisccfg.dll"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MD /W3 /GX /O2 /I "./" /I "../../../" /I "include" /I "../include" /I "../../../lib/isc/win32" /I "../../../lib/isc/win32/include" /I "../../../lib/isc/include" /I "../../../lib/dns/win32/include" /I "../../../lib/dns/include" /I "../..../lib/dns/sec/openssl/include" /I "../../../lib/dns/sec/dst/include" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "__STDC__" /D "_MBCS" /D "_USRDLL" /D "USE_MD5" /D "OPENSSL" /D "DST_USE_PRIVATE_OPENSSL" /D "LIBISCCFG_EXPORTS" /Fp"$(INTDIR)\libisccfg.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
MTL_PROJ=/nologo /D "NDEBUG" /mktyplib203 /win32 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\libisccfg.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=user32.lib advapi32.lib ws2_32.lib ../../isc/win32/Release/libisc.lib ../../dns/win32/Release/libdns.lib /nologo /dll /incremental:no /pdb:"$(OUTDIR)\libisccfg.pdb" /machine:I386 /def:".\libisccfg.def" /out:"../../../Build/Release/libisccfg.dll" /implib:"$(OUTDIR)\libisccfg.lib" 
DEF_FILE= \
	".\libisccfg.def"
LINK32_OBJS= \
	"$(INTDIR)\check.obj" \
	"$(INTDIR)\DLLMain.obj" \
	"$(INTDIR)\log.obj" \
	"$(INTDIR)\parser.obj" \
	"$(INTDIR)\version.obj" \
	"..\..\isc\win32\Release\libisc.lib" \
	"..\..\dns\win32\Release\libdns.lib"

"..\..\..\Build\Release\libisccfg.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "libisccfg - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "..\..\..\Build\Debug\libisccfg.dll" "$(OUTDIR)\libisccfg.bsc"

!ELSE 

ALL : "libdns - Win32 Debug" "libisc - Win32 Debug" "..\..\..\Build\Debug\libisccfg.dll" "$(OUTDIR)\libisccfg.bsc"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"libisc - Win32 DebugCLEAN" "libdns - Win32 DebugCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\check.obj"
	-@erase "$(INTDIR)\check.sbr"
	-@erase "$(INTDIR)\DLLMain.obj"
	-@erase "$(INTDIR)\DLLMain.sbr"
	-@erase "$(INTDIR)\log.obj"
	-@erase "$(INTDIR)\log.sbr"
	-@erase "$(INTDIR)\parser.obj"
	-@erase "$(INTDIR)\parser.sbr"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(INTDIR)\version.obj"
	-@erase "$(INTDIR)\version.sbr"
	-@erase "$(OUTDIR)\libisccfg.bsc"
	-@erase "$(OUTDIR)\libisccfg.exp"
	-@erase "$(OUTDIR)\libisccfg.lib"
	-@erase "$(OUTDIR)\libisccfg.pdb"
	-@erase "..\..\..\Build\Debug\libisccfg.dll"
	-@erase "..\..\..\Build\Debug\libisccfg.ilk"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MDd /W3 /Gm /GX /ZI /Od /I "./" /I "../../../" /I "include" /I "../include" /I "../../../lib/isc/win32" /I "../../../lib/isc/win32/include" /I "../../../lib/isc/include" /I "../../../lib/dns/win32/include" /I "../../../lib/dns/include" /I "../..../lib/dns/sec/openssl/include" /I "../../../lib/dns/sec/dst/include" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "LIBISCCFG_EXPORTS" /FR"$(INTDIR)\\" /Fp"$(INTDIR)\libisccfg.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 
MTL_PROJ=/nologo /D "_DEBUG" /mktyplib203 /win32 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\libisccfg.bsc" 
BSC32_SBRS= \
	"$(INTDIR)\check.sbr" \
	"$(INTDIR)\DLLMain.sbr" \
	"$(INTDIR)\log.sbr" \
	"$(INTDIR)\parser.sbr" \
	"$(INTDIR)\version.sbr"

"$(OUTDIR)\libisccfg.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
LINK32_FLAGS=user32.lib advapi32.lib ws2_32.lib ../../isc/win32/debug/libisc.lib ../../dns/win32/debug/libdns.lib /nologo /dll /incremental:yes /pdb:"$(OUTDIR)\libisccfg.pdb" /debug /machine:I386 /def:".\libisccfg.def" /out:"../../../Build/Debug/libisccfg.dll" /implib:"$(OUTDIR)\libisccfg.lib" /pdbtype:sept 
DEF_FILE= \
	".\libisccfg.def"
LINK32_OBJS= \
	"$(INTDIR)\check.obj" \
	"$(INTDIR)\DLLMain.obj" \
	"$(INTDIR)\log.obj" \
	"$(INTDIR)\parser.obj" \
	"$(INTDIR)\version.obj" \
	"..\..\isc\win32\Debug\libisc.lib" \
	"..\..\dns\win32\Debug\libdns.lib"

"..\..\..\Build\Debug\libisccfg.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
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
!IF EXISTS("libisccfg.dep")
!INCLUDE "libisccfg.dep"
!ELSE 
!MESSAGE Warning: cannot find "libisccfg.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "libisccfg - Win32 Release" || "$(CFG)" == "libisccfg - Win32 Debug"
SOURCE=..\check.c

!IF  "$(CFG)" == "libisccfg - Win32 Release"


"$(INTDIR)\check.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "libisccfg - Win32 Debug"


"$(INTDIR)\check.obj"	"$(INTDIR)\check.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=.\DLLMain.c

!IF  "$(CFG)" == "libisccfg - Win32 Release"


"$(INTDIR)\DLLMain.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "libisccfg - Win32 Debug"


"$(INTDIR)\DLLMain.obj"	"$(INTDIR)\DLLMain.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=..\log.c

!IF  "$(CFG)" == "libisccfg - Win32 Release"


"$(INTDIR)\log.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "libisccfg - Win32 Debug"


"$(INTDIR)\log.obj"	"$(INTDIR)\log.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\parser.c

!IF  "$(CFG)" == "libisccfg - Win32 Release"


"$(INTDIR)\parser.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "libisccfg - Win32 Debug"


"$(INTDIR)\parser.obj"	"$(INTDIR)\parser.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=.\version.c

!IF  "$(CFG)" == "libisccfg - Win32 Release"


"$(INTDIR)\version.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "libisccfg - Win32 Debug"


"$(INTDIR)\version.obj"	"$(INTDIR)\version.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

!IF  "$(CFG)" == "libisccfg - Win32 Release"

"libisc - Win32 Release" : 
   cd "..\..\isc\win32"
   $(MAKE) /$(MAKEFLAGS) /F ".\libisc.mak" CFG="libisc - Win32 Release" 
   cd "..\..\isccfg\win32"

"libisc - Win32 ReleaseCLEAN" : 
   cd "..\..\isc\win32"
   $(MAKE) /$(MAKEFLAGS) /F ".\libisc.mak" CFG="libisc - Win32 Release" RECURSE=1 CLEAN 
   cd "..\..\isccfg\win32"

!ELSEIF  "$(CFG)" == "libisccfg - Win32 Debug"

"libisc - Win32 Debug" : 
   cd "..\..\isc\win32"
   $(MAKE) /$(MAKEFLAGS) /F ".\libisc.mak" CFG="libisc - Win32 Debug" 
   cd "..\..\isccfg\win32"

"libisc - Win32 DebugCLEAN" : 
   cd "..\..\isc\win32"
   $(MAKE) /$(MAKEFLAGS) /F ".\libisc.mak" CFG="libisc - Win32 Debug" RECURSE=1 CLEAN 
   cd "..\..\isccfg\win32"

!ENDIF 

!IF  "$(CFG)" == "libisccfg - Win32 Release"

"libdns - Win32 Release" : 
   cd "..\..\dns\win32"
   $(MAKE) /$(MAKEFLAGS) /F ".\libdns.mak" CFG="libdns - Win32 Release" 
   cd "..\..\isccfg\win32"

"libdns - Win32 ReleaseCLEAN" : 
   cd "..\..\dns\win32"
   $(MAKE) /$(MAKEFLAGS) /F ".\libdns.mak" CFG="libdns - Win32 Release" RECURSE=1 CLEAN 
   cd "..\..\isccfg\win32"

!ELSEIF  "$(CFG)" == "libisccfg - Win32 Debug"

"libdns - Win32 Debug" : 
   cd "..\..\dns\win32"
   $(MAKE) /$(MAKEFLAGS) /F ".\libdns.mak" CFG="libdns - Win32 Debug" 
   cd "..\..\isccfg\win32"

"libdns - Win32 DebugCLEAN" : 
   cd "..\..\dns\win32"
   $(MAKE) /$(MAKEFLAGS) /F ".\libdns.mak" CFG="libdns - Win32 Debug" RECURSE=1 CLEAN 
   cd "..\..\isccfg\win32"

!ENDIF 


!ENDIF 

