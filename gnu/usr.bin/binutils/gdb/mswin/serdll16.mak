PROJ = serdll16
DEBUG = 1
CC = cl -I.
RC = rc

D_RCDEFINES = -d_DEBUG
R_RCDEFINES = -dNDEBUG
CFLAGS_D_WDLL = /nologo /W3 /FR /G2 /Zi /D_DEBUG /Od /GD /ALw /Fd"serdll16.PDB"
CFLAGS_R_WDLL = /nologo /W3 /FR /O1 /DNDEBUG /GD /ALw
LFLAGS_D_WDLL = /NOLOGO /ONERROR:NOEXE /NOD /PACKC:61440 /CO /NOE /ALIGN:16 /MAP:FULL
LFLAGS_R_WDLL = /NOLOGO /ONERROR:NOEXE /NOD /PACKC:61440 /NOE /ALIGN:16 /MAP:FULL

LIBS_D_WDLL = oldnames libw commdlg ldllcew
LIBS_R_WDLL = oldnames libw commdlg ldllcew

RCFLAGS = /nologo
RESFLAGS = /nologo

DEFFILE = $(PROJ).DEF

!if "$(DEBUG)" == "1"
CFLAGS = $(CFLAGS_D_WDLL)
LFLAGS = $(LFLAGS_D_WDLL)
LIBS = $(LIBS_D_WDLL)
MAPFILE = nul
RCDEFINES = $(D_RCDEFINES)
!else
CFLAGS = $(CFLAGS_R_WDLL)
LFLAGS = $(LFLAGS_R_WDLL)
LIBS = $(LIBS_R_WDLL)
MAPFILE = nul
RCDEFINES = $(R_RCDEFINES)
!endif

!if [if exist MSVC.BND del MSVC.BND]
!endif
SBRS = $(PROJ).SBR


all:    $(PROJ).DLL $(PROJ).BSC
	copy $(PROJ).dll g:\serdll16.dll

$(PROJ).OBJ:       $(PROJ).C 
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c $(PROJ).C


$(PROJ).DLL::   $(PROJ).OBJ $(OBJS_EXT) $(DEFFILE)
	echo >NUL @<<$(PROJ).CRF
$(PROJ).OBJ +
$(OBJS_EXT)
$(PROJ).DLL
$(MAPFILE)
W32SUT16.LIB+
$(LIBS)
$(DEFFILE);
<<
	link $(LFLAGS) @$(PROJ).CRF
	$(RC) $(RESFLAGS) $@
	implib /nowep $(PROJ).LIB $(PROJ).DLL


run: $(PROJ).DLL
	$(PROJ) $(RUNFLAGS)


$(PROJ).BSC: $(SBRS)
	bscmake @<<
/o$@ $(SBRS)
<<
