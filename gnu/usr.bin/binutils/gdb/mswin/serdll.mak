
#====================== serdll.mak ============================

# Make file for serdll32.c -> serdll32.dll
# and for serdll16.c -> serdll16.dll

!include "config.mak"

#====================== WinNT serial support ============================

!IF "$(HOST)" == "WINNT" || "$(HOST)" == "WIN31"

!include <ntwin32.mak>

all: setmsvc32 $(OUTDIR)\serdll32.dll 

# Update the object file if necessary
# $* - path/name of target
# $@ - path/name.ext of target
# $< - path/name.ext of dep

setmsvc32:
	@set INIT=$(MSVC32)\;
	@set PATH=$(MSVC32)\BIN;$(MSVC32)\BIN\WIN95;y:\BIN;C:\WINDOWS;C:\WINDOWS\COMMAND;y:\BIN\CD;y:\BIN\NT;y:\BIN\MKSNT;
	@set INCLUDE=$(MSVC32)\INCLUDE;$(MSVC32)\MFC\INCLUDE
	@set LIB=$(MSVC32)\LIB;$(MSVC32)\MFC\LIB

$(INTDIR)\serdll32.obj: $(MSWINDIR)\serdll32.c $(MSWINDIR)\serdll32.h setmsvc32
	@echo ======= building serdll32.obj =========
    $(cc) $(cflags) $(cvarsdll) $(cdebug) $(COMMON_INCS) $(MSWINDIR)\serdll32.c /Fo$@ $(CCOPTS)


$(OUTDIR)\serdll32.lib $(OUTDIR)\serdll32.exp: $(INTDIR)\serdll32.obj $(MSWINDIR)\serdll32.def setmsvc32
	@echo ======= building serdll32.lib & serdll32.exp =========
     $(implib) -machine:$(CPU) -def:$(MSWINDIR)\serdll32.def $(INTDIR)\serdll32.obj -out:$*.lib

$(OUTDIR)\serdll32.dll: $(INTDIR)\serdll32.obj $(MSWINDIR)\serdll32.def $(OUTDIR)\serdll32.exp setmsvc32
	@echo ======= building serdll32.dll =========
    $(link) $(conflags) $(ldebug) -dll -entry:DllInit$(DLLENTRY) -base:0x20000000 -out:$@ $(OUTDIR)\serdll32.exp $(INTDIR)\serdll32.obj $(guilibsdll) $(MSWINDIR)\w32sut32.lib mpr.lib

!ENDIF ###"$(HOST)" == "WINNT" || "$(HOST)" == "WIN31"
#========================================================================


#====================== Win3.1 with Win32s serial support ==================
!IF "$(HOST)" == "WIN31"

PROJ16 = serdll16
DEBUG = 1
CC16 = $(MSVC16)\bin\cl -I. $(COMMON_INCS) /Fo$(INTDIR)\serdll16.obj /Fr$(INTDIR)\serdll16.sbr
RC16 = $(MSVC16)\bin\rc
LINK16 = $(MSVC16)\bin\link 
IMPLIB16 = $(MSVC16)\bin\implib 
BSCMAKE16 = $(MSVC16)\bin\bscmake

#/ALw gets "conflict memory size" error; trying /AL
#MEMMOD16 = /ALw

D_RCDEFINES16 = -d_DEBUG
R_RCDEFINES16 = -dNDEBUG
CFLAGS_D_WDLL16 = /nologo /W3 /G2 /Zi /D_DEBUG /Od /GD $(MEMMOD16) /Fd$(OUTDIR)\"serdll16.PDB" $(CCOPTS)
CFLAGS_R_WDLL16 = /nologo /W3 /O1 /DNDEBUG /GD $(MEMMOD16) $(CCOPTS)
LFLAGS_D_WDLL16 = /NOLOGO /ONERROR:NOEXE /NOD /PACKC:61440 /CO /NOE /ALIGN:16 /MAP:FULL
LFLAGS_R_WDLL16 = /NOLOGO /ONERROR:NOEXE /NOD /PACKC:61440 /NOE /ALIGN:16 /MAP:FULL

LIBS_D_WDLL16 = oldnames libw commdlg ldllcew
LIBS_R_WDLL16 = oldnames libw commdlg ldllcew

RCFLAGS16 = /nologo
RESFLAGS16 = /nologo

DEFFILE16 = $(MSWINDIR)\$(PROJ16).DEF

!if "$(DEBUG)" == "1"
CFLAGS16 = $(CFLAGS_D_WDLL16)
LFLAGS16 = $(LFLAGS_D_WDLL16)
LIBS16 = $(LIBS_D_WDLL16)
MAPFILE = nul
RCDEFINES16 = $(D_RCDEFINES16)
!else
CFLAGS16 = $(CFLAGS_R_WDLL16)
LFLAGS16 = $(LFLAGS_R_WDLL16)
LIBS16 = $(LIBS_R_WDLL16)
MAPFILE = nul
RCDEFINES16 = $(R_RCDEFINES16)
!endif

!if [if exist MSVC.BND del MSVC.BND]
!endif
SBRS16 = $(INTDIR)\$(PROJ16).SBR


all16:  setmsvc16 $(OUTDIR)\$(PROJ16).DLL $(OUTDIR)\$(PROJ16).BSC


setmsvc16:
	@set INIT=$(MSVC16)\;
	@set PATH=$(MSVC16)\BIN;$(MSVC16)\BIN\WIN95;y:\BIN;C:\WINDOWS;C:\WINDOWS\COMMAND;y:\BIN\CD;y:\BIN\NT;y:\BIN\MKSNT;
	@set INCLUDE=$(MSVC16)\INCLUDE;$(MSVC16)\MFC\INCLUDE
	@set LIB=$(MSVC16)\LIB;$(MSVC16)\MFC\LIB

$(INTDIR)\$(PROJ16).OBJ: $(MSWINDIR)\$(PROJ16).c setmsvc16
	@echo ======= building serdll16.obj =========
	rem $(CC16) $(CFLAGS16) $(CUSEPCHFLAG16) /c $(MSWINDIR)\$(PROJ16).c
	$(CC16) @<<
$(MSWINDIR)\$(PROJ16).C $(CFLAGS16) $(CUSEPCHFLAG16) /c  $(CCOPTS)
<<


$(OUTDIR)\$(PROJ16).DLL:: $(INTDIR)\$(PROJ16).OBJ $(OBJS_EXT) $(DEFFILE16) setmsvc16
	@echo ======= building serdll16.dll =========
	echo >NUL @<<$(PROJ16).CRF
$(INTDIR)\$(PROJ16).OBJ +
$(OBJS_EXT)
$(OUTDIR)\$(PROJ16).DLL
$(MAPFILE)
$(MSWINDIR)\W32SUT16.LIB+
$(LIBS16)
$(DEFFILE16);
<<
	$(LINK16) $(LFLAGS16) @$(PROJ16).CRF
	$(RC16) $(RESFLAGS16) $@

$(OUTDIR)\$(PROJ16).LIB: $(OUTDIR)\$(PROJ16).DLL $(DEFFILE16) setmsvc16
	@echo ======= building serdll16.lib =========
	$(IMPLIB16) /nowep $(OUTDIR)\$(PROJ16).LIB $(OUTDIR)\$(PROJ16).DLL


run: $(OUTDIR)\$(PROJ16).DLL
	$(OUTDIR)\$(PROJ16) $(RUNFLAGS16)


$(OUTDIR)\$(PROJ16).BSC: $(SBRS16) setmsvc16
	$(BSCMAKE16) @<<
/o$@ $(SBRS16)
<<

!ENDIF ###"$(HOST)" == "WIN31"
#========================================================================


#====================== Win95 serial support ============================
!IF "$(HOST)" == "WIN95"

!IF "$(CFG)" == ""
CFG=Win32 Debug
!ENDIF 

#MTL=MkTypLib.exe
CPP=cl.exe
RSC=rc.exe

# /MT was included in the options below...
#SER_CFLAGS=/MT $(SER_CFLAGS)

!IF  "$(CFG)" == "Win32 Release"

ALL : setmsvc32 $(OUTDIR)/serdll32.dll $(OUTDIR)/serdll32.bsc

$(OUTDIR) : 
    if not exist $(OUTDIR)/nul mkdir $(OUTDIR)

MTL_PROJ=/nologo /D "NDEBUG" /win32 
CPP_PROJ=/nologo $(SER_CFLAGS) /W3 /GX /YX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"serdll32.pch" /Fo$(INTDIR)/ /c $(COMMON_INCS)
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o$(OUTDIR)/"serdll32.bsc" 
BSC32_SBRS= \
	$(INTDIR)/serdll32.sbr

$(OUTDIR)/serdll32.bsc : $(OUTDIR)  $(BSC32_SBRS) setmsvc32
	@echo ======= building serdll32.bsc =========
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib /NOLOGO /SUBSYSTEM:windows /DLL /INCREMENTAL:no\
 /PDB:$(OUTDIR)/"serdll32.pdb" /MACHINE:I386 /DEF:"$(MSWINDIR)\serdll32.def"\
 /OUT:$(OUTDIR)/"serdll32.dll" /IMPLIB:$(OUTDIR)/"serdll32.lib" 

DEF_FILE=$(MSWINDIR)\serdll32.def
LINK32_OBJS= \
	$(INTDIR)/serdll32.obj

$(OUTDIR)/serdll32.dll : $(OUTDIR)  $(DEF_FILE) $(LINK32_OBJS) setmsvc32
	@echo ======= building serdll32.dll =========
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "Win32 Debug"

ALL : setmsvc32 $(OUTDIR)/serdll32.dll $(OUTDIR)/serdll32.bsc

$(OUTDIR) : 
    if not exist $(OUTDIR)/nul mkdir $(OUTDIR)

MTL_PROJ=/nologo /D "_DEBUG" /win32 
CPP_PROJ=/nologo $(SER_CFLAGS) /W3 /GX /Zi /YX /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"serdll32.pch" /Fo$(INTDIR)/\
 /Fd$(OUTDIR)/"serdll32.pdb" /c $(COMMON_INCS)
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o$(OUTDIR)/"serdll32.bsc" 
BSC32_SBRS= \
	$(INTDIR)/serdll32.sbr

$(OUTDIR)/serdll32.bsc : $(OUTDIR)  $(BSC32_SBRS) setmsvc32
	@echo ======= building serdll32.bsc =========
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib /NOLOGO /SUBSYSTEM:windows /DLL /INCREMENTAL:yes\
 /PDB:$(OUTDIR)/"serdll32.pdb" /DEBUG /MACHINE:I386 /DEF:"$(MSWINDIR)\serdll32.def"\
 /OUT:$(OUTDIR)/"serdll32.dll" /IMPLIB:$(OUTDIR)/"serdll32.lib" 

DEF_FILE=$(MSWINDIR)\serdll32.def
LINK32_OBJS= \
	$(INTDIR)/serdll32.obj

$(OUTDIR)/serdll32.dll : $(OUTDIR)  $(DEF_FILE) $(LINK32_OBJS) setmsvc32
	@echo ======= building serdll32.dll =========
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 

setmsvc32:
	@set INIT=$(MSVC32)\;
	@set PATH=$(MSVC32)\BIN;$(MSVC32)\BIN\WIN95;y:\BIN;C:\WINDOWS;C:\WINDOWS\COMMAND;y:\BIN\CD;y:\BIN\NT;y:\BIN\MKSNT;
	@set INCLUDE=$(MSVC32)\INCLUDE;$(MSVC32)\MFC\INCLUDE
	@set LIB=$(MSVC32)\LIB;$(MSVC32)\MFC\LIB


{$(MSWINDIR)}.c{$(INTDIR)}.obj:
   $(CPP) $(CPP_PROJ) $(CCOPTS) $<  

{$(MSWINDIR)}.cpp{$(INTDIR)}.obj:
   $(CPP) $(CPP_PROJ) $(CCOPTS) $<  

{$(MSWINDIR)}.cxx{$(INTDIR)}.obj: 
   $(CPP) $(CPP_PROJ) $(CCOPTS) $<  

SOURCE=$(MSWINDIR)\serdll32.c

$(INTDIR)/serdll32.obj : $(SOURCE) $(INTDIR) setmsvc32

!ENDIF ###"$(HOST)" == "WIN95"
#========================================================================

#==================== sertest ===================================

SERTEST=sertest
#SERTEST_DEBUG=/Fd$(OUTDIR)\sertest.pdb
#SERTEST_LDEBUG=-PDB:$(OUTDIR)\sertest.pdb -DEBUG -MAP:$(OUTDIR)\sertest.map   

sertest : $(OUTDIR)\$(SERTEST).exe 

SERTEST_OBJS= \
    $(INTDIR)\sertest.obj \
    $(INTDIR)\serstub.obj \
    $(INTDIR)\serial.obj \
    $(INTDIR)\vasprintf.obj \
    $(INTDIR)\ser-win32s.obj \
    $(INTDIR)\debugo.obj 

!IF "$(HOST)" == "WIN95"
#SERDLL=serdll95
SERDLL=serdll32
!ENDIF ###"$(HOST)" == "WIN95"
!IF "$(HOST)" == "WINNT" || "$(HOST)" == "WIN31"
SERDLL=serdll32
!ENDIF ###"$(HOST)" == "WIN95"

SERTEST_LIBS= \
    $(MSVC)\lib\oldnames.lib \
    $(OUTDIR)\$(SERDLL).lib

$(OUTDIR)\$(SERTEST).exe : $(OUTDIR) $(INTDIR) $(INTDIR)/sim/nul $(SERTEST_OBJS) $(SERTEST_LIBS)
        @echo ======= linking $(OUTDIR)\$(SERTEST).exe ======
    $(LINK32) @<<
  $(SERTEST_LFLAGS) $(LFLAGS) $(SERTEST_OBJS) $(SERTEST_LIBS) $(TARGET_LFLAGS) /OUT:$(OUTDIR)\$(SERTEST).exe /SUBSYSTEM:console
<<

$(INTDIR)\sertest.obj : $(SRCDIR)\gdb\mswin\sertest.c
	$(CPP) $(CPP_PROJ) $(CCOPTS) $(SRCDIR)\gdb\mswin\sertest.c
$(INTDIR)\serstub.obj : $(SRCDIR)\gdb\mswin\serstub.c
	$(CPP) $(CPP_PROJ) $(CCOPTS) $(SRCDIR)\gdb\mswin\serstub.c
$(INTDIR)\debugo.obj : $(SRCDIR)\libiberty\debugo.c
	$(CPP) $(CPP_PROJ) $(CCOPTS) $(SRCDIR)\libiberty\debugo.c
$(INTDIR)\vasprintf.obj : $(SRCDIR)\libiberty\vasprintf.c
	$(CPP) $(CPP_PROJ) $(CCOPTS) $(SRCDIR)\libiberty\vasprintf.c
$(INTDIR)\serial.obj : $(SRCDIR)\gdb\serial.c
	$(CPP) $(CPP_PROJ) $(CCOPTS) $(SRCDIR)\gdb\serial.c
$(INTDIR)\ser-win32s.obj : $(SRCDIR)\gdb\mswin\ser-win32s.c
	$(CPP) $(CPP_PROJ) $(CCOPTS) $(SRCDIR)\gdb\mswin\ser-win32s.c
