# Microsoft Developer Studio Generated NMAKE File, Based on voodoo.dsp
!IF "$(CFG)" == ""
CFG=voodoo - Win32 Release
!MESSAGE No configuration specified. Defaulting to voodoo - Win32 Release.
!ENDIF 

!IF "$(CFG)" != "voodoo - Win32 Release" && "$(CFG)" != "voodoo - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line.  For example:
!MESSAGE 
!MESSAGE NMAKE /f "voodoo.mak" CFG="voodoo - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "voodoo - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "voodoo - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "voodoo - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\.\Release
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\voodoo.exe" "$(OUTDIR)\voodoo.bsc"

!ELSE 

ALL : "KrbManager - Win32 Release" "$(OUTDIR)\voodoo.exe"\
 "$(OUTDIR)\voodoo.bsc"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"KrbManager - Win32 ReleaseCLEAN" 
!ELSE 
CLEAN : 
!ENDIF 
	-@erase "$(INTDIR)\AuthOption.obj"
	-@erase "$(INTDIR)\AuthOption.sbr"
	-@erase "$(INTDIR)\CharStream.obj"
	-@erase "$(INTDIR)\CharStream.sbr"
	-@erase "$(INTDIR)\CryptoEngine.obj"
	-@erase "$(INTDIR)\CryptoEngine.sbr"
	-@erase "$(INTDIR)\DenyAllOption.obj"
	-@erase "$(INTDIR)\DenyAllOption.sbr"
	-@erase "$(INTDIR)\EmulatorEngine.obj"
	-@erase "$(INTDIR)\EmulatorEngine.sbr"
	-@erase "$(INTDIR)\EncryptOption.obj"
	-@erase "$(INTDIR)\EncryptOption.sbr"
	-@erase "$(INTDIR)\Negotiator.obj"
	-@erase "$(INTDIR)\Negotiator.sbr"
	-@erase "$(INTDIR)\Option.obj"
	-@erase "$(INTDIR)\Option.sbr"
	-@erase "$(INTDIR)\TelnetApp.obj"
	-@erase "$(INTDIR)\TelnetApp.sbr"
	-@erase "$(INTDIR)\TelnetEngine.obj"
	-@erase "$(INTDIR)\TelnetEngine.sbr"
	-@erase "$(INTDIR)\TelnetResource.res"
	-@erase "$(INTDIR)\TelnetSession.obj"
	-@erase "$(INTDIR)\TelnetSession.sbr"
	-@erase "$(INTDIR)\TerminalEngine.obj"
	-@erase "$(INTDIR)\TerminalEngine.sbr"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(INTDIR)\WinSizeOption.obj"
	-@erase "$(INTDIR)\WinSizeOption.sbr"
	-@erase "$(INTDIR)\YesNoOptions.obj"
	-@erase "$(INTDIR)\YesNoOptions.sbr"
	-@erase "$(OUTDIR)\voodoo.bsc"
	-@erase "$(OUTDIR)\voodoo.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /ML /W3 /GX /O2 /I "..\..\lib\kclient" /I "..\..\lib\krb" /I\
 "..\..\lib\des" /I "..\..\include" /I "..\..\include\win32" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FR"$(INTDIR)\\" /Fp"$(INTDIR)\voodoo.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
CPP_OBJS=.\Release/
CPP_SBRS=.\Release/

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
MTL_PROJ=/nologo /D "NDEBUG" /mktyplib203 /win32 
RSC=rc.exe
RSC_PROJ=/l 0x409 /fo"$(INTDIR)\TelnetResource.res" /d "NDEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\voodoo.bsc" 
BSC32_SBRS= \
	"$(INTDIR)\AuthOption.sbr" \
	"$(INTDIR)\CharStream.sbr" \
	"$(INTDIR)\CryptoEngine.sbr" \
	"$(INTDIR)\DenyAllOption.sbr" \
	"$(INTDIR)\EmulatorEngine.sbr" \
	"$(INTDIR)\EncryptOption.sbr" \
	"$(INTDIR)\Negotiator.sbr" \
	"$(INTDIR)\Option.sbr" \
	"$(INTDIR)\TelnetApp.sbr" \
	"$(INTDIR)\TelnetEngine.sbr" \
	"$(INTDIR)\TelnetSession.sbr" \
	"$(INTDIR)\TerminalEngine.sbr" \
	"$(INTDIR)\WinSizeOption.sbr" \
	"$(INTDIR)\YesNoOptions.sbr"

"$(OUTDIR)\voodoo.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<
	
LINK32=link.exe
LINK32_FLAGS=..\..\lib\kclient\Release\kclnt32.lib\
 ..\..\lib\krb\Release\krb.lib ..\..\lib\des\Release\des.lib wsock32.lib\
 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib\
 shell32.lib ole32.lib oleaut32.lib uuid.lib /nologo /subsystem:windows\
 /incremental:no /pdb:"$(OUTDIR)\voodoo.pdb" /machine:I386\
 /out:"$(OUTDIR)\voodoo.exe" 
LINK32_OBJS= \
	"$(INTDIR)\AuthOption.obj" \
	"$(INTDIR)\CharStream.obj" \
	"$(INTDIR)\CryptoEngine.obj" \
	"$(INTDIR)\DenyAllOption.obj" \
	"$(INTDIR)\EmulatorEngine.obj" \
	"$(INTDIR)\EncryptOption.obj" \
	"$(INTDIR)\Negotiator.obj" \
	"$(INTDIR)\Option.obj" \
	"$(INTDIR)\TelnetApp.obj" \
	"$(INTDIR)\TelnetEngine.obj" \
	"$(INTDIR)\TelnetResource.res" \
	"$(INTDIR)\TelnetSession.obj" \
	"$(INTDIR)\TerminalEngine.obj" \
	"$(INTDIR)\WinSizeOption.obj" \
	"$(INTDIR)\YesNoOptions.obj"

"$(OUTDIR)\voodoo.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "voodoo - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\.\Debug
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\voodoo.exe" "$(OUTDIR)\voodoo.bsc"

!ELSE 

ALL : "KrbManager - Win32 Debug" "$(OUTDIR)\voodoo.exe" "$(OUTDIR)\voodoo.bsc"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"KrbManager - Win32 DebugCLEAN" 
!ELSE 
CLEAN : 
!ENDIF 
	-@erase "$(INTDIR)\AuthOption.obj"
	-@erase "$(INTDIR)\AuthOption.sbr"
	-@erase "$(INTDIR)\CharStream.obj"
	-@erase "$(INTDIR)\CharStream.sbr"
	-@erase "$(INTDIR)\CryptoEngine.obj"
	-@erase "$(INTDIR)\CryptoEngine.sbr"
	-@erase "$(INTDIR)\DenyAllOption.obj"
	-@erase "$(INTDIR)\DenyAllOption.sbr"
	-@erase "$(INTDIR)\EmulatorEngine.obj"
	-@erase "$(INTDIR)\EmulatorEngine.sbr"
	-@erase "$(INTDIR)\EncryptOption.obj"
	-@erase "$(INTDIR)\EncryptOption.sbr"
	-@erase "$(INTDIR)\Negotiator.obj"
	-@erase "$(INTDIR)\Negotiator.sbr"
	-@erase "$(INTDIR)\Option.obj"
	-@erase "$(INTDIR)\Option.sbr"
	-@erase "$(INTDIR)\TelnetApp.obj"
	-@erase "$(INTDIR)\TelnetApp.sbr"
	-@erase "$(INTDIR)\TelnetEngine.obj"
	-@erase "$(INTDIR)\TelnetEngine.sbr"
	-@erase "$(INTDIR)\TelnetResource.res"
	-@erase "$(INTDIR)\TelnetSession.obj"
	-@erase "$(INTDIR)\TelnetSession.sbr"
	-@erase "$(INTDIR)\TerminalEngine.obj"
	-@erase "$(INTDIR)\TerminalEngine.sbr"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(INTDIR)\vc50.pdb"
	-@erase "$(INTDIR)\WinSizeOption.obj"
	-@erase "$(INTDIR)\WinSizeOption.sbr"
	-@erase "$(INTDIR)\YesNoOptions.obj"
	-@erase "$(INTDIR)\YesNoOptions.sbr"
	-@erase "$(OUTDIR)\voodoo.bsc"
	-@erase "$(OUTDIR)\voodoo.exe"
	-@erase "$(OUTDIR)\voodoo.ilk"
	-@erase "$(OUTDIR)\voodoo.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MDd /W4 /Gm /GX /Zi /Od /I "..\..\lib\kclient" /I\
 "..\..\lib\krb" /I "..\..\lib\des" /I "..\..\include" /I "..\..\include\win32"\
 /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /FR"$(INTDIR)\\" /Fp"$(INTDIR)\voodoo.pch"\
 /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
CPP_OBJS=.\Debug/
CPP_SBRS=.\Debug/

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
MTL_PROJ=/nologo /D "_DEBUG" /mktyplib203 /win32 
RSC=rc.exe
RSC_PROJ=/l 0x409 /fo"$(INTDIR)\TelnetResource.res" /d "_DEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\voodoo.bsc" 
BSC32_SBRS= \
	"$(INTDIR)\AuthOption.sbr" \
	"$(INTDIR)\CharStream.sbr" \
	"$(INTDIR)\CryptoEngine.sbr" \
	"$(INTDIR)\DenyAllOption.sbr" \
	"$(INTDIR)\EmulatorEngine.sbr" \
	"$(INTDIR)\EncryptOption.sbr" \
	"$(INTDIR)\Negotiator.sbr" \
	"$(INTDIR)\Option.sbr" \
	"$(INTDIR)\TelnetApp.sbr" \
	"$(INTDIR)\TelnetEngine.sbr" \
	"$(INTDIR)\TelnetSession.sbr" \
	"$(INTDIR)\TerminalEngine.sbr" \
	"$(INTDIR)\WinSizeOption.sbr" \
	"$(INTDIR)\YesNoOptions.sbr"

"$(OUTDIR)\voodoo.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
LINK32_FLAGS=..\..\lib\kclient\Debug\kclnt32.lib ..\..\lib\krb\Debug\krb.lib\
 ..\..\lib\des\Debug\des.lib wsock32.lib kernel32.lib user32.lib gdi32.lib\
 winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib\
 uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /incremental:yes\
 /pdb:"$(OUTDIR)\voodoo.pdb" /debug /machine:I386 /out:"$(OUTDIR)\voodoo.exe" 
LINK32_OBJS= \
	"$(INTDIR)\AuthOption.obj" \
	"$(INTDIR)\CharStream.obj" \
	"$(INTDIR)\CryptoEngine.obj" \
	"$(INTDIR)\DenyAllOption.obj" \
	"$(INTDIR)\EmulatorEngine.obj" \
	"$(INTDIR)\EncryptOption.obj" \
	"$(INTDIR)\Negotiator.obj" \
	"$(INTDIR)\Option.obj" \
	"$(INTDIR)\TelnetApp.obj" \
	"$(INTDIR)\TelnetEngine.obj" \
	"$(INTDIR)\TelnetResource.res" \
	"$(INTDIR)\TelnetSession.obj" \
	"$(INTDIR)\TerminalEngine.obj" \
	"$(INTDIR)\WinSizeOption.obj" \
	"$(INTDIR)\YesNoOptions.obj"

"$(OUTDIR)\voodoo.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 


!IF "$(CFG)" == "voodoo - Win32 Release" || "$(CFG)" == "voodoo - Win32 Debug"
SOURCE=.\AuthOption.cpp
DEP_CPP_AUTHO=\
	"..\..\include\win32\ktypes.h"\
	"..\..\lib\des\des.h"\
	"..\..\lib\kclient\KClient.h"\
	"..\..\lib\krb\krb-protos.h"\
	"..\..\lib\krb\krb.h"\
	".\AuthOption.h"\
	".\CharStream.h"\
	".\CryptoEngine.h"\
	".\DenyAllOption.h"\
	".\Negotiator.h"\
	".\Option.h"\
	".\TelnetApp.h"\
	".\TelnetCodes.h"\
	".\TelnetEngine.h"\
	".\TelnetSession.h"\


"$(INTDIR)\AuthOption.obj"	"$(INTDIR)\AuthOption.sbr" : $(SOURCE)\
 $(DEP_CPP_AUTHO) "$(INTDIR)"


SOURCE=.\CharStream.cpp
DEP_CPP_CHARS=\
	".\CharStream.h"\
	

"$(INTDIR)\CharStream.obj"	"$(INTDIR)\CharStream.sbr" : $(SOURCE)\
 $(DEP_CPP_CHARS) "$(INTDIR)"


SOURCE=.\CryptoEngine.cpp
DEP_CPP_CRYPT=\
	"..\..\lib\des\des.h"\
	".\CryptoEngine.h"\
	

"$(INTDIR)\CryptoEngine.obj"	"$(INTDIR)\CryptoEngine.sbr" : $(SOURCE)\
 $(DEP_CPP_CRYPT) "$(INTDIR)"


SOURCE=.\DenyAllOption.cpp
DEP_CPP_DENYA=\
	".\CharStream.h"\
	".\DenyAllOption.h"\
	".\Option.h"\
	".\TelnetCodes.h"\
	

"$(INTDIR)\DenyAllOption.obj"	"$(INTDIR)\DenyAllOption.sbr" : $(SOURCE)\
 $(DEP_CPP_DENYA) "$(INTDIR)"


SOURCE=.\EmulatorEngine.cpp
DEP_CPP_EMULA=\
	"..\..\lib\des\des.h"\
	"..\..\lib\kclient\KClient.h"\
	".\char_codes.h"\
	".\CharStream.h"\
	".\CryptoEngine.h"\
	".\DenyAllOption.h"\
	".\EmulatorEngine.h"\
	".\Negotiator.h"\
	".\Option.h"\
	".\TelnetApp.h"\
	".\TelnetCodes.h"\
	".\TelnetEngine.h"\
	".\TelnetSession.h"\
	".\TerminalEngine.h"\
	".\WinSizeOption.h"\
	".\YesNoOptions.h"\
	

"$(INTDIR)\EmulatorEngine.obj"	"$(INTDIR)\EmulatorEngine.sbr" : $(SOURCE)\
 $(DEP_CPP_EMULA) "$(INTDIR)"


SOURCE=.\EncryptOption.cpp
DEP_CPP_ENCRY=\
	"..\..\lib\des\des.h"\
	"..\..\lib\kclient\KClient.h"\
	".\CharStream.h"\
	".\CryptoEngine.h"\
	".\DenyAllOption.h"\
	".\EmulatorEngine.h"\
	".\EncryptOption.h"\
	".\Negotiator.h"\
	".\Option.h"\
	".\TelnetCodes.h"\
	".\TelnetEngine.h"\
	".\TelnetSession.h"\
	".\TerminalEngine.h"\
	

"$(INTDIR)\EncryptOption.obj"	"$(INTDIR)\EncryptOption.sbr" : $(SOURCE)\
 $(DEP_CPP_ENCRY) "$(INTDIR)"


SOURCE=.\Negotiator.cpp
DEP_CPP_NEGOT=\
	".\CharStream.h"\
	".\DenyAllOption.h"\
	".\Negotiator.h"\
	".\Option.h"\
	".\TelnetCodes.h"\
	

"$(INTDIR)\Negotiator.obj"	"$(INTDIR)\Negotiator.sbr" : $(SOURCE)\
 $(DEP_CPP_NEGOT) "$(INTDIR)"


SOURCE=.\Option.cpp
DEP_CPP_OPTIO=\
	"..\..\lib\des\des.h"\
	"..\..\lib\kclient\KClient.h"\
	".\CharStream.h"\
	".\CryptoEngine.h"\
	".\DenyAllOption.h"\
	".\Negotiator.h"\
	".\Option.h"\
	".\Telnet.h"\
	".\TelnetCodes.h"\
	".\TelnetEngine.h"\
	

"$(INTDIR)\Option.obj"	"$(INTDIR)\Option.sbr" : $(SOURCE) $(DEP_CPP_OPTIO)\
 "$(INTDIR)"


SOURCE=.\TelnetApp.cpp
DEP_CPP_TELNE=\
	"..\..\lib\des\des.h"\
	"..\..\lib\kclient\KClient.h"\
	".\CharStream.h"\
	".\CryptoEngine.h"\
	".\DenyAllOption.h"\
	".\Negotiator.h"\
	".\Option.h"\
	".\TelnetApp.h"\
	".\TelnetCodes.h"\
	".\TelnetEngine.h"\
	".\TelnetSession.h"\
	".\TerminalEngine.h"\
	

"$(INTDIR)\TelnetApp.obj"	"$(INTDIR)\TelnetApp.sbr" : $(SOURCE)\
 $(DEP_CPP_TELNE) "$(INTDIR)"


SOURCE=.\TelnetEngine.cpp
DEP_CPP_TELNET=\
	"..\..\lib\des\des.h"\
	"..\..\lib\kclient\KClient.h"\
	".\AuthOption.h"\
	".\CharStream.h"\
	".\CryptoEngine.h"\
	".\DenyAllOption.h"\
	".\EmulatorEngine.h"\
	".\EncryptOption.h"\
	".\Negotiator.h"\
	".\Option.h"\
	".\Telnet.h"\
	".\TelnetApp.h"\
	".\TelnetCodes.h"\
	".\TelnetEngine.h"\
	".\TelnetSession.h"\
	".\TerminalEngine.h"\
	".\WinSizeOption.h"\
	".\YesNoOptions.h"\
	

"$(INTDIR)\TelnetEngine.obj"	"$(INTDIR)\TelnetEngine.sbr" : $(SOURCE)\
 $(DEP_CPP_TELNET) "$(INTDIR)"


SOURCE=.\TelnetResource.rc

"$(INTDIR)\TelnetResource.res" : $(SOURCE) "$(INTDIR)"
	$(RSC) $(RSC_PROJ) $(SOURCE)


SOURCE=.\TelnetSession.cpp
DEP_CPP_TELNETS=\
	"..\..\lib\des\des.h"\
	"..\..\lib\kclient\KClient.h"\
	".\CharStream.h"\
	".\CryptoEngine.h"\
	".\DenyAllOption.h"\
	".\EmulatorEngine.h"\
	".\EncryptOption.h"\
	".\Negotiator.h"\
	".\Option.h"\
	".\Telnet.h"\
	".\TelnetApp.h"\
	".\TelnetCodes.h"\
	".\TelnetEngine.h"\
	".\TelnetSession.h"\
	".\TerminalEngine.h"\
	".\WinSizeOption.h"\
	".\YesNoOptions.h"\
	

"$(INTDIR)\TelnetSession.obj"	"$(INTDIR)\TelnetSession.sbr" : $(SOURCE)\
 $(DEP_CPP_TELNETS) "$(INTDIR)"


SOURCE=.\TerminalEngine.cpp
DEP_CPP_TERMI=\
	"..\..\lib\des\des.h"\
	"..\..\lib\kclient\KClient.h"\
	".\char_codes.h"\
	".\CharStream.h"\
	".\CryptoEngine.h"\
	".\DenyAllOption.h"\
	".\EmulatorEngine.h"\
	".\Negotiator.h"\
	".\Option.h"\
	".\TelnetApp.h"\
	".\TelnetCodes.h"\
	".\TelnetEngine.h"\
	".\TelnetSession.h"\
	".\TerminalEngine.h"\
	

"$(INTDIR)\TerminalEngine.obj"	"$(INTDIR)\TerminalEngine.sbr" : $(SOURCE)\
 $(DEP_CPP_TERMI) "$(INTDIR)"


SOURCE=.\WinSizeOption.cpp
DEP_CPP_WINSI=\
	"..\..\lib\des\des.h"\
	"..\..\lib\kclient\KClient.h"\
	".\CharStream.h"\
	".\CryptoEngine.h"\
	".\DenyAllOption.h"\
	".\EmulatorEngine.h"\
	".\Negotiator.h"\
	".\Option.h"\
	".\TelnetCodes.h"\
	".\TelnetEngine.h"\
	".\TelnetSession.h"\
	".\TerminalEngine.h"\
	".\WinSizeOption.h"\
	".\YesNoOptions.h"\
	

"$(INTDIR)\WinSizeOption.obj"	"$(INTDIR)\WinSizeOption.sbr" : $(SOURCE)\
 $(DEP_CPP_WINSI) "$(INTDIR)"


SOURCE=.\YesNoOptions.cpp
DEP_CPP_YESNO=\
	".\CharStream.h"\
	".\Option.h"\
	".\TelnetCodes.h"\
	".\YesNoOptions.h"\
	

"$(INTDIR)\YesNoOptions.obj"	"$(INTDIR)\YesNoOptions.sbr" : $(SOURCE)\
 $(DEP_CPP_YESNO) "$(INTDIR)"


!IF  "$(CFG)" == "voodoo - Win32 Release"

"KrbManager - Win32 Release" : 
   cd "\tmp\wirus-krb\krb4-pre-0.9.9\appl\krbmanager"
   $(MAKE) /$(MAKEFLAGS) /F ".\krbmanager.mak" CFG="KrbManager - Win32 Release"\

   cd "..\voodoo"

"KrbManager - Win32 ReleaseCLEAN" : 
   cd "\tmp\wirus-krb\krb4-pre-0.9.9\appl\krbmanager"
   $(MAKE) /$(MAKEFLAGS) CLEAN /F ".\krbmanager.mak"\
 CFG="KrbManager - Win32 Release" RECURSE=1 
   cd "..\voodoo"

!ELSEIF  "$(CFG)" == "voodoo - Win32 Debug"

"KrbManager - Win32 Debug" : 
   cd "\tmp\wirus-krb\krb4-pre-0.9.9\appl\krbmanager"
   $(MAKE) /$(MAKEFLAGS) /F ".\krbmanager.mak" CFG="KrbManager - Win32 Debug" 
   cd "..\voodoo"

"KrbManager - Win32 DebugCLEAN" : 
   cd "\tmp\wirus-krb\krb4-pre-0.9.9\appl\krbmanager"
   $(MAKE) /$(MAKEFLAGS) CLEAN /F ".\krbmanager.mak"\
 CFG="KrbManager - Win32 Debug" RECURSE=1 
   cd "..\voodoo"

!ENDIF 


!ENDIF 

