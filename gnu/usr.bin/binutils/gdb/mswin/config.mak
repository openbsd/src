
# change these settings as needed for your environment

# output files will be copied to OUTDIR (see settings below)

# set TARGET in including makefile
# set HOST to WIN31 WINNT WIN95

!IF "$(FROM_MAKEFILE)" != "makefile"

!IF "$(TARGET)" == ""
TARGET=sh
!MESSAGE TARGET defaulting to $(TARGET)
!ENDIF

!IF "$(HOST)" == ""
HOST=WINNT
!MESSAGE HOST defaulting to $(HOST)
!ENDIF

!IF "$(REL)" == ""
REL=963
!MESSAGE REL defaulting to $(REL)
!ENDIF

!IF "$(DEBUG)" == ""
DEBUG=ON
!MESSAGE DEBUG defaulting to $(DEBUG)
!ENDIF

!IF "$(SRCDIR)" == ""
SRCDIR=\devo
!MESSAGE SRCDIR defaulting to $(SRCDIR)
!ENDIF

!IF "$(BLDDIR)" == ""
BLDDIR=\gs
!MESSAGE BLDDIR defaulting to $(BLDDIR)
!ENDIF

!IF "$(MSVC32)" == ""
MSVC32=c:\msvc22
!MESSAGE MSVC32 defaulting to $(MSVC32)
!ENDIF

!IF "$(MSVC16)" == ""
MSVC16=c:\msvc15
!MESSAGE MSVC16 defaulting to $(MSVC16)
!ENDIF

!IF "$(MSTOOLS)" == ""
MSTOOLS=c:\mstools
!MESSAGE MSTOOLS defaulting to $(MSTOOLS)
!ENDIF

#names of targets to create
!IF "$(GDB)" == ""
GDB=gdb
!ENDIF
!IF "$(SIM)" == ""
SIM=sim
!ENDIF
!IF "$(SER)" == ""
SER=serdll32
!ENDIF

#UNIXDIR=y:\bin\mksnt
CPU=i386
MSWINDIR=$(SRCDIR)\gdb\mswin
PREDIR=$(SRCDIR)\gdb\mswin\prebuilt
OUTDIR=$(BLDDIR)\$(TARGET)
INTDIR=$(BLDDIR)\tmp\$(TARGET)
EXE=$(OUTDIR)\$(GDB).exe
BSC=$(OUTDIR)\$(GDB).bsc
NMS=$(OUTDIR)\$(GDB).nms
MSVC=$(MSVC32)

!ENDIF


# debug flags

  # build Win32 Debug by default

!IF "$(DEBUG)" != "" || "$(CFG)" != "Win32 Release"
CFG=Win32 Debug

CDEBUG= /Zi -D _DEBUG /FR$(INTDIR)/ /Od -D DEBUGO
SIM_DEBUG= /Fd$(OUTDIR)/$(SIM).pdb 
SIM_DEBUG16= /Fd$(OUTDIR)/$(SIM)16.pdb 
GDB_DEBUG = /Fd$(OUTDIR)/$(GDB).pdb 
GDB_DEBUG16 = /Fd$(OUTDIR)/$(GDB)16.pdb 
SER_DEBUG= /Fd$(OUTDIR)/$(SER).pdb 
SER_DEBUG16= /Fd$(OUTDIR)/$(SER)16.pdb 

LDEBUG= -DEBUG -INCREMENTAL:yes
LDEBUG16= /CODEVIEW /MAP
SIM_LDEBUG= /PDB:$(OUTDIR)\$(SIM).pdb /MAP:$(OUTDIR)\$(SIM).map
SIM_LDEBUG16=
GDB_LDEBUG= /PDB:$(OUTDIR)\$(GDB).pdb /MAP:$(OUTDIR)\$(GDB).map
GDB_LDEBUG16=
SER_LDEBUG= /PDB:$(OUTDIR)/$(SER).pdb /MAP:$(OUTDIR)\$(SER).map
SER_LDEBUG16= /PDB:$(OUTDIR)/$(SER)16.pdb /MAP:$(OUTDIR)\$(SER)16.map

!ELSE
CFG=Win32 Release

CDEBUG= /O2 /D "NDEBUG"
GDB_DEBUG=
SIM_DEBUG=
SER_DEBUG=
LDEBUG= -INCREMENTAL:no
GDB_LDEBUG=
SIM_LDEBUG=
SER_LDEBUG=
!ENDIF

# target specific flags
# some of these should go with the common flags

TARGET_CFLAGS= -I $(PREDIR)/$(TARGET) $(TARGET_CFLAGS) 
TARGET_CFLAGS16 = /W2 /Zp $(TARGET_CFLAGS16) $(TARGET_CFLAGS)
TARGET_CFLAGS32 = $(TARGET_CFLAGS32) $(TARGET_CFLAGS)

#TARGET_LFLAGS= $(TARGET_LFLAGS) 
TARGET_LFLAGS32 = $(MSVC)\lib\oldnames.lib $(TARGET_LFLAGS32) $(TARGET_LFLAGS)
TARGET_LFLAGS16 = $(TARGET_LFLAGS16) $(TARGET_LFLAGS)
TARGET_LIBS_DLL16= $(MSVC16)\lib\oldnames $(MSVC16)\lib\ldllcew $(MSVC16)\lib\libw


#info for debugging makefiles
#    to debug, use nmake DSET=set DECHO=echo DTYPE=type VERBOSE=yes

!IF "$(VERBOSE)" != ""
DSET=set
DECHO=echo
DTYPE=type
LOGO=-v
!ELSE
!IF "$(DECHO)" == ""
DECHO=rem
!ENDIF
!IF "$(DTYPE)" == ""
DTYPE=rem
!ENDIF
!IF "$(DSET)" == ""
DSET=rem
!ENDIF
!IF "$(LOGO)"==""
LOGO=-nologo 
!ENDIF
!ENDIF

# set makefile variables

CPP = $(MSVC)\bin\cl.exe
CC = $(MSVC)\bin\cl.exe
LINK32 = $(MSVC)\bin\link.exe
AR = $(MSVC)\bin\lib.exe
IMPLIB = $(MSVC)\bin\lib.exe
MTL=$(MSVC)\bin\MkTypLib.exe
RSC=$(MSVC)\bin\rc.exe
BSC32=$(MSVC)\bin\bscmake.exe

CPP16 = $(MSVC16)\bin\cl.exe
CC16 = $(MSVC16)\bin\cl.exe
LINK16 = $(MSVC16)\bin\link.exe
AR16 = $(MSVC16)\bin\lib.exe
IMPLIB16 = $(MSVC16)\bin\implib.exe

!IF "$(MAKEPATH)" == ""
MAKEPATH=$(PATH)
!ENDIF

INCLUDE16=$(MSVC16)\INCLUDE;$(MSVC16)\MFC\INCLUDE;
LIB16=$(MSVC16)\LIB;$(MSVC16)\MFC\LIB;
PATH16=$(MSVC16)\BIN;$(MSVC16)\BIN\WIN95;$(MAKEPATH)
INCLUDE32=$(MSVC32)\INCLUDE;$(MSVC32)\MFC\INCLUDE;
LIB32=$(MSVC32)\LIB;$(MSVC32)\MFC\LIB;
PATH32=$(MSVC32)\BIN;$(MSVC32)\BIN\WIN95;$(MAKEPATH)


# compile and link flags

# Explanation of various non-obvious compiler flags
#
# -G4	optimize for 80486
# -MD	link with MSVCRT.LIB
# -W3	set warning level to 3 (default 1)
# -GX	enable C++ EH (exception handling)
# -YX	automatic .PCH (pre-compiled headers)
# -Od	disable optimizations
# -Ow   assume cross-function aliasing
# -Fd	specify name of .pdb file (debug information database)
# -Fp	specify name of precompiled header file
# -Fo	specify name of object file (if ends in slash, it's a directory)
# -Zi	generate debug information (.pdb file)
# -Zp   pack bytes on n byte boundary (what's default??)



# compiler flags
#additional new compiler flags (extra /Zi)
#/Ow /W2 /Zp /Zi -I h:\gnu\devo\include 

#previous releases require different defines
!IF "$(REL)" == "963"
REL_CFLAGS32= -D WIN32 -D __WIN32__ 
!ELSE
REL_CFLAGS= -D __STDC__=1
!ENDIF

MMALLOC_CFLAGS = -D NO_MMCHECK

COMMON_INCS = -I $(MSWINDIR) -I $(SRCDIR)/gdb/config \
	-I $(PREDIR) -I $(SRCDIR)/mmalloc \
 -I $(PREDIR)/libiberty -I $(SRCDIR)/bfd \
 -I $(SRCDIR)/libiberty -I $(SRCDIR)/include -I $(SRCDIR)/readline -I $(SRCDIR)\gdb -I $(PREDIR)/$(TARGET)

COMMON_DEFS = -D _WINDOWS \
 -D ALMOST_STDC -D NO_SYS_PARAM -D _MBCS -D NEED_basename \
 -D _POSIX_ -D HAVE_CONFIG_H 

COMMON_CFLAGS = $(LOGO) -W3 -YX -c $(COMMON_DEFS) $(COMMON_INCS) $(CDEBUG) $(REL_CFLAGS)
COMMON_CFLAGS32 = -G4 -GX $(COMMON_CFLAGS) -D _WIN32 $(REL_CFLAGS32)
COMMON_CFLAGS16 = /ALw /Gsw /Ow $(COMMON_CFLAGS) 

GDB_CFLAGS = $(GDB_DEBUG) -Fp$(OUTDIR)/$(GDB).pch -Fo$(INTDIR)/ 
GDB_CFLAGS16 = $(GDB_DEBUG16) -Fp$(OUTDIR)/$(GDB)16.pch -Fo$(INTDIR)/ 
SIM_CFLAGS = $(SIM_DEBUG) -DINSIDE_SIMULATOR -Fp$(OUTDIR)/$(SIM).pch -Fo$(INTDIR)/sim/ 
SIM_CFLAGS16 = $(SIM_DEBUG16) -DINSIDE_SIMULATOR -Fp$(OUTDIR)/$(SIM)16.pch -Fo$(INTDIR)/sim/ 
SER_CFLAGS = /Ow /Zp $(SER_DEBUG) /Fp$(OUTDIR)/$(SER).pch /Fo$(INTDIR)/ 
SER_CFLAGS16 = $(SER_DEBUG16) /Fp$(OUTDIR)/serdll16.pch /Fo$(INTDIR)/ 

CFLAGS = $(COMMON_CFLAGS32) $(TARGET_CFLAGS32) $(MMALLOC_CFLAGS) $(CCOPTS)
CFLAGS16 = $(COMMON_CFLAGS16) $(TARGET_CFLAGS16) $(MMALLOC_CFLAGS) $(CCOPTS)

CFLAGS_DLL = $(COMMON_CFLAGS32) $(TARGET_CFLAGS32) $(MMALLOC_CFLAGS) $(CCOPTS)
CFLAGS_DLL16 = $(COMMON_CFLAGS16) $(TARGET_CFLAGS16) $(MMALLOC_CFLAGS) $(CCOPTS)


# linker flags
COMMON_LFLAGS16= /CO /NOD /ONERROR:NOEXE $(LDEBUG16)
COMMON_LFLAGS32=$(LOGO) \
 $(MSVC)/lib/kernel32.lib $(MSVC)/lib/user32.lib \
 -MACHINE:I386 $(LDEBUG)

GDB_LFLAGS= $(GDB_LDEBUG) -SUBSYSTEM:windows
GDB_LFLAGS16= $(GDB_LDEBUG16) 
SIM_LFLAGS= $(SIM_LDEBUG) 
SIM_LFLAGS16= $(SIM_LDEBUG16) 
SER_LFLAGS= $(SER_LDEBUG)
SER_LFLAGS16= $(SER_LDEBUG16)
 
LFLAGS = $(COMMON_LFLAGS32) $(TARGET_LFLAGS32) $(LDOPTS)
LFLAGS16 = $(COMMON_LFLAGS16) $(TARGET_LFLAGS16) $(LDOPTS)
LIBS_DLL=gdi32.lib winspool.lib comdlg32.lib \
	advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib \
	odbc32.lib odbccp32.lib 
LFLAGS_DLL=$(COMMON_LFLAGS32) /DLL /INCREMENTAL:yes $(LIBS_DLL)
LFLAGS_DLL16=$(COMMON_LFLAGS16) /DLL $(LIBS_DLL)


# and even more flags

BSC32_FLAGS=$(LOGO)
RSC_FLAGS=/l 0x409 /d "NDEBUG"


# misc flags set in win32.mak and ntwin32.mak
#conflags=
#ldebug=
#guilibsdll=w32sut32.lib mpr.lib
#dll_lflags= -dll -entry:DllInit$(DLLENTRY) -base:0x20000000 $(guilibsdll) w32sut32.lib mpr.lib

