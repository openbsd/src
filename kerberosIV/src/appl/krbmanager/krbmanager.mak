# Microsoft Developer Studio Generated NMAKE File, Based on KrbManager.dsp
!IF "$(CFG)" == ""
CFG=KrbManager - Win32 Release
!MESSAGE No configuration specified. Defaulting to KrbManager - Win32 Release.
!ENDIF 

!IF "$(CFG)" != "KrbManager - Win32 Release" && "$(CFG)" !=\
 "KrbManager - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "KrbManager.mak" CFG="KrbManager - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "KrbManager - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "KrbManager - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "KrbManager - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\.\Release
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\KrbManager.exe"

!ELSE 

ALL : "$(OUTDIR)\KrbManager.exe"

!ENDIF 

CLEAN :
	-@erase "$(INTDIR)\krbmanager.obj"
	-@erase "$(INTDIR)\krbmanager.res"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(OUTDIR)\KrbManager.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /ML /W3 /O1 /I "..\..\lib\krb" /I "..\..\lib\des" /I\
 "..\..\include" /I "..\..\include\win32" /D "NDEBUG" /D "WIN32" /D "_WINDOWS"\
 /D "HAVE_CONFIG_H" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
CPP_OBJS=.\Release/
CPP_SBRS=.

.c{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

MTL=midl.exe
MTL_PROJ=/nologo /D "NDEBUG" /mktyplib203 /o NUL /win32 
RSC=rc.exe
RSC_PROJ=/l 0x41d /fo"$(INTDIR)\krbmanager.res" /d "NDEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\KrbManager.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib comctl32.lib\
 ..\..\lib\krb\Release\krb.lib ..\..\lib\des\Release\des.lib\
 ..\..\lib\roken\Release\roken.lib /nologo /version:1.2 /subsystem:windows\
 /pdb:none /machine:I386 /out:"$(OUTDIR)\KrbManager.exe" 
LINK32_OBJS= \
	"$(INTDIR)\krbmanager.obj" \
	"$(INTDIR)\krbmanager.res"

"$(OUTDIR)\KrbManager.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "KrbManager - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\.\Debug
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\KrbManager.exe"

!ELSE 

ALL : "$(OUTDIR)\KrbManager.exe"

!ENDIF 

CLEAN :
	-@erase "$(INTDIR)\krbmanager.obj"
	-@erase "$(INTDIR)\krbmanager.res"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(INTDIR)\vc50.pdb"
	-@erase "$(OUTDIR)\KrbManager.exe"
	-@erase "$(OUTDIR)\KrbManager.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MLd /W3 /Gm /Zi /Od /I "..\..\lib\krb" /I "..\..\lib\des" /I\
 "..\..\include" /I "..\..\include\win32" /D "_DEBUG" /D "WIN32" /D "_WINDOWS"\
 /D "HAVE_CONFIG_H" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
CPP_OBJS=.\Debug/
CPP_SBRS=.

.c{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

MTL=midl.exe
MTL_PROJ=/nologo /D "_DEBUG" /mktyplib203 /o NUL /win32 
RSC=rc.exe
RSC_PROJ=/l 0x41d /fo"$(INTDIR)\krbmanager.res" /d "_DEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\KrbManager.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib comctl32.lib\
 ..\..\lib\krb\Debug\krb.lib ..\..\lib\des\Debug\des.lib\
 ..\..\lib\roken\Debug\roken.lib /nologo /subsystem:windows /incremental:no\
 /pdb:"$(OUTDIR)\KrbManager.pdb" /debug /machine:I386\
 /out:"$(OUTDIR)\KrbManager.exe" /pdbtype:sept 
LINK32_OBJS= \
	"$(INTDIR)\krbmanager.obj" \
	"$(INTDIR)\krbmanager.res"

"$(OUTDIR)\KrbManager.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 


!IF "$(CFG)" == "KrbManager - Win32 Release" || "$(CFG)" ==\
 "KrbManager - Win32 Debug"
SOURCE=.\krbmanager.c

!IF  "$(CFG)" == "KrbManager - Win32 Release"

DEP_CPP_KRBMA=\
	"..\..\include\win32\ktypes.h"\
	"..\..\lib\des\des.h"\
	"..\..\lib\krb\krb-protos.h"\
	"..\..\lib\krb\krb.h"\
	

"$(INTDIR)\krbmanager.obj" : $(SOURCE) $(DEP_CPP_KRBMA) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "KrbManager - Win32 Debug"

DEP_CPP_KRBMA=\
	"..\..\include\win32\ktypes.h"\
	"..\..\lib\des\des.h"\
	"..\..\lib\krb\krb-protos.h"\
	"..\..\lib\krb\krb.h"\
	

"$(INTDIR)\krbmanager.obj" : $(SOURCE) $(DEP_CPP_KRBMA) "$(INTDIR)"


!ENDIF 

SOURCE=.\krbmanager.rc
DEP_RSC_KRBMAN=\
	".\res\KrbManager.ico"\
	".\res\krbmanager.rc2"\
	".\res\red_green.bmp"\
	

"$(INTDIR)\krbmanager.res" : $(SOURCE) $(DEP_RSC_KRBMAN) "$(INTDIR)"
	$(RSC) $(RSC_PROJ) $(SOURCE)



!ENDIF 

# Microsoft Developer Studio Generated NMAKE File, Based on krbmanager.dsp
!IF "$(CFG)" == ""
CFG=KrbManager - Win32 Release
!MESSAGE No configuration specified. Defaulting to KrbManager - Win32 Release.
!ENDIF 

!IF "$(CFG)" != "KrbManager - Win32 Release" && "$(CFG)" !=\
 "KrbManager - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line.  For example:
!MESSAGE 
!MESSAGE NMAKE /f "krbmanager.mak" CFG="KrbManager - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "KrbManager - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "KrbManager - Win32 Debug" (based on "Win32 (x86) Application")
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

!IF  "$(CFG)" == "KrbManager - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\.\Release
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\krbmanager.exe"

!ELSE 

ALL : "kclient - Win32 Release" "$(OUTDIR)\krbmanager.exe"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"kclient - Win32 ReleaseCLEAN" 
!ELSE 
CLEAN : 
!ENDIF 
	-@erase "$(INTDIR)\Doc.obj"
	-@erase "$(INTDIR)\KrbManager.obj"
	-@erase "$(INTDIR)\krbmanager.pch"
	-@erase "$(INTDIR)\krbmanager.res"
	-@erase "$(INTDIR)\MainFrm.obj"
	-@erase "$(INTDIR)\StdAfx.obj"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(INTDIR)\View.obj"
	-@erase "$(OUTDIR)\krbmanager.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MD /W3 /GX /O2 /I "..\..\lib\krb" /I "..\..\lib\des" /I\
 "..\..\include" /I "..\..\include\win32" /D "NDEBUG" /D "WIN32" /D "_WINDOWS"\
 /D "_AFXDLL" /D "_MBCS" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\krbmanager.pch"\
 /Yu"stdafx.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
CPP_OBJS=.\Release/
CPP_SBRS=.
MTL_PROJ=/nologo /D "NDEBUG" /mktyplib203 /win32 
RSC_PROJ=/l 0x41d /fo"$(INTDIR)\krbmanager.res" /d "NDEBUG" /d "_AFXDLL" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\krbmanager.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=..\..\lib\krb\Release\krb.lib ..\..\lib\des\Release\des.lib\
 ..\..\lib\roken\Release\roken.lib /nologo /subsystem:windows /incremental:no\
 /pdb:"$(OUTDIR)\krbmanager.pdb" /machine:I386 /out:"$(OUTDIR)\krbmanager.exe" 
LINK32_OBJS= \
	"$(INTDIR)\Doc.obj" \
	"$(INTDIR)\KrbManager.obj" \
	"$(INTDIR)\krbmanager.res" \
	"$(INTDIR)\MainFrm.obj" \
	"$(INTDIR)\StdAfx.obj" \
	"$(INTDIR)\View.obj" \
	"..\..\lib\kclient\Release\kclnt32.lib"

"$(OUTDIR)\krbmanager.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "KrbManager - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\.\Debug
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\krbmanager.exe" "$(OUTDIR)\krbmanager.bsc"

!ELSE 

ALL : "kclient - Win32 Debug" "$(OUTDIR)\krbmanager.exe"\
 "$(OUTDIR)\krbmanager.bsc"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"kclient - Win32 DebugCLEAN" 
!ELSE 
CLEAN : 
!ENDIF 
	-@erase "$(INTDIR)\Doc.obj"
	-@erase "$(INTDIR)\Doc.sbr"
	-@erase "$(INTDIR)\KrbManager.obj"
	-@erase "$(INTDIR)\krbmanager.pch"
	-@erase "$(INTDIR)\krbmanager.res"
	-@erase "$(INTDIR)\KrbManager.sbr"
	-@erase "$(INTDIR)\MainFrm.obj"
	-@erase "$(INTDIR)\MainFrm.sbr"
	-@erase "$(INTDIR)\StdAfx.obj"
	-@erase "$(INTDIR)\StdAfx.sbr"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(INTDIR)\vc50.pdb"
	-@erase "$(INTDIR)\View.obj"
	-@erase "$(INTDIR)\View.sbr"
	-@erase "$(OUTDIR)\krbmanager.bsc"
	-@erase "$(OUTDIR)\krbmanager.exe"
	-@erase "$(OUTDIR)\krbmanager.ilk"
	-@erase "$(OUTDIR)\krbmanager.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MDd /W3 /Gm /GX /Zi /Od /I "..\..\lib\krb" /I "..\..\lib\des"\
 /I "..\..\include" /I "..\..\include\win32" /D "_DEBUG" /D "WIN32" /D\
 "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /D "HAVE_CONFIG_H" /FR"$(INTDIR)\\"\
 /Fp"$(INTDIR)\krbmanager.pch" /Yu"stdafx.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\"\
 /FD /c 
CPP_OBJS=.\Debug/
CPP_SBRS=.\Debug/
MTL_PROJ=/nologo /D "_DEBUG" /mktyplib203 /win32 
RSC_PROJ=/l 0x41d /fo"$(INTDIR)\krbmanager.res" /d "_DEBUG" /d "_AFXDLL" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\krbmanager.bsc" 
BSC32_SBRS= \
	"$(INTDIR)\Doc.sbr" \
	"$(INTDIR)\KrbManager.sbr" \
	"$(INTDIR)\MainFrm.sbr" \
	"$(INTDIR)\StdAfx.sbr" \
	"$(INTDIR)\View.sbr"

"$(OUTDIR)\krbmanager.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
LINK32_FLAGS=..\..\lib\krb\Debug\krb.lib ..\..\lib\des\Debug\des.lib\
 ..\..\lib\roken\Debug\roken.lib /nologo /subsystem:windows /incremental:yes\
 /pdb:"$(OUTDIR)\krbmanager.pdb" /debug /machine:I386\
 /out:"$(OUTDIR)\krbmanager.exe" 
LINK32_OBJS= \
	"$(INTDIR)\Doc.obj" \
	"$(INTDIR)\KrbManager.obj" \
	"$(INTDIR)\krbmanager.res" \
	"$(INTDIR)\MainFrm.obj" \
	"$(INTDIR)\StdAfx.obj" \
	"$(INTDIR)\View.obj" \
	"..\..\lib\kclient\Debug\kclnt32.lib"

"$(OUTDIR)\krbmanager.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 

.c{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<


!IF "$(CFG)" == "KrbManager - Win32 Release" || "$(CFG)" ==\
 "KrbManager - Win32 Debug"
SOURCE=.\Doc.cpp
DEP_CPP_DOC_C=\
	".\Doc.h"\
	".\KrbManager.h"\
	".\StdAfx.h"\


!IF  "$(CFG)" == "KrbManager - Win32 Release"


"$(INTDIR)\Doc.obj" : $(SOURCE) $(DEP_CPP_DOC_C) "$(INTDIR)"\
 "$(INTDIR)\krbmanager.pch"


!ELSEIF  "$(CFG)" == "KrbManager - Win32 Debug"


"$(INTDIR)\Doc.obj"	"$(INTDIR)\Doc.sbr" : $(SOURCE) $(DEP_CPP_DOC_C)\
 "$(INTDIR)" "$(INTDIR)\krbmanager.pch"


!ENDIF 

SOURCE=.\KrbManager.cpp
DEP_CPP_KRBMA=\
	".\Doc.h"\
	".\KrbManager.h"\
	".\MainFrm.h"\
	".\StdAfx.h"\
	".\View.h"\
	

!IF  "$(CFG)" == "KrbManager - Win32 Release"


"$(INTDIR)\KrbManager.obj" : $(SOURCE) $(DEP_CPP_KRBMA) "$(INTDIR)"\
 "$(INTDIR)\krbmanager.pch"


!ELSEIF  "$(CFG)" == "KrbManager - Win32 Debug"


"$(INTDIR)\KrbManager.obj"	"$(INTDIR)\KrbManager.sbr" : $(SOURCE)\
 $(DEP_CPP_KRBMA) "$(INTDIR)" "$(INTDIR)\krbmanager.pch"


!ENDIF 

SOURCE=.\MainFrm.cpp
DEP_CPP_MAINF=\
	".\KrbManager.h"\
	".\MainFrm.h"\
	".\StdAfx.h"\
	".\View.h"\
	

!IF  "$(CFG)" == "KrbManager - Win32 Release"


"$(INTDIR)\MainFrm.obj" : $(SOURCE) $(DEP_CPP_MAINF) "$(INTDIR)"\
 "$(INTDIR)\krbmanager.pch"
	
	
!ELSEIF  "$(CFG)" == "KrbManager - Win32 Debug"


"$(INTDIR)\MainFrm.obj"	"$(INTDIR)\MainFrm.sbr" : $(SOURCE) $(DEP_CPP_MAINF)\
 "$(INTDIR)" "$(INTDIR)\krbmanager.pch"


!ENDIF 

SOURCE=.\StdAfx.cpp
DEP_CPP_STDAF=\
	".\StdAfx.h"\
	

!IF  "$(CFG)" == "KrbManager - Win32 Release"

CPP_SWITCHES=/nologo /MD /W3 /GX /O2 /I "..\..\lib\krb" /I "..\..\lib\des" /I\
 "..\..\include" /I "..\..\include\win32" /D "NDEBUG" /D "WIN32" /D "_WINDOWS"\
 /D "_AFXDLL" /D "_MBCS" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\krbmanager.pch"\
 /Yc"stdafx.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\StdAfx.obj"	"$(INTDIR)\krbmanager.pch" : $(SOURCE) $(DEP_CPP_STDAF)\
 "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "KrbManager - Win32 Debug"

CPP_SWITCHES=/nologo /MDd /W3 /Gm /GX /Zi /Od /I "..\..\lib\krb" /I\
 "..\..\lib\des" /I "..\..\include" /I "..\..\include\win32" /D "_DEBUG" /D\
 "WIN32" /D "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /D "HAVE_CONFIG_H"\
 /FR"$(INTDIR)\\" /Fp"$(INTDIR)\krbmanager.pch" /Yc"stdafx.h" /Fo"$(INTDIR)\\"\
 /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\StdAfx.obj"	"$(INTDIR)\StdAfx.sbr"	"$(INTDIR)\krbmanager.pch" : \
$(SOURCE) $(DEP_CPP_STDAF) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\View.cpp
DEP_CPP_VIEW_=\
	"..\..\include\win32\ktypes.h"\
	"..\..\lib\des\des.h"\
	"..\..\lib\krb\krb-protos.h"\
	"..\..\lib\krb\krb.h"\
	".\Doc.h"\
	".\KrbManager.h"\
	".\StdAfx.h"\
	".\View.h"\
	

!IF  "$(CFG)" == "KrbManager - Win32 Release"


"$(INTDIR)\View.obj" : $(SOURCE) $(DEP_CPP_VIEW_) "$(INTDIR)"\
 "$(INTDIR)\krbmanager.pch"


!ELSEIF  "$(CFG)" == "KrbManager - Win32 Debug"


"$(INTDIR)\View.obj"	"$(INTDIR)\View.sbr" : $(SOURCE) $(DEP_CPP_VIEW_)\
 "$(INTDIR)" "$(INTDIR)\krbmanager.pch"


!ENDIF 

SOURCE=.\krbmanager.rc
DEP_RSC_KRBMAN=\
	".\res\KrbManager.ico"\
	".\res\krbmanager.rc2"\
	".\res\red_green.bmp"\
	

"$(INTDIR)\krbmanager.res" : $(SOURCE) $(DEP_RSC_KRBMAN) "$(INTDIR)"
   $(RSC) $(RSC_PROJ) $(SOURCE)


!IF  "$(CFG)" == "KrbManager - Win32 Release"

"kclient - Win32 Release" : 
   cd "\TEMP\fj4\lib\kclient"
   $(MAKE) /$(MAKEFLAGS) /F .\KClient.mak CFG="kclient - Win32 Release" 
   cd "..\..\appl\krbmanager"

"kclient - Win32 ReleaseCLEAN" : 
   cd "\TEMP\fj4\lib\kclient"
   $(MAKE) /$(MAKEFLAGS) CLEAN /F .\KClient.mak CFG="kclient - Win32 Release"\
 RECURSE=1 
   cd "..\..\appl\krbmanager"

!ELSEIF  "$(CFG)" == "KrbManager - Win32 Debug"

"kclient - Win32 Debug" : 
   cd "\TEMP\fj4\lib\kclient"
   $(MAKE) /$(MAKEFLAGS) /F .\KClient.mak CFG="kclient - Win32 Debug" 
   cd "..\..\appl\krbmanager"

"kclient - Win32 DebugCLEAN" : 
   cd "\TEMP\fj4\lib\kclient"
   $(MAKE) /$(MAKEFLAGS) CLEAN /F .\KClient.mak CFG="kclient - Win32 Debug"\
 RECURSE=1 
   cd "..\..\appl\krbmanager"

!ENDIF 


!ENDIF 

