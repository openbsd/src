

# Master nakefile for building wingdb targets

# Change these settings as needed for your environment:
# you can also set them on the cmd line via nmake VAR=val
# To build all targets for release on NT, do:
#	nmake BUILD=Release HOST=WINNT TARGET=all

# dawn's prefered settings:
!IF "$(PREFS)" == "dawn"
SRCDIR=h:\gnu\devo
OUTDIR=h:\gs\sh
INTDIR=h:\gs\tmp\sh
HOST=WIN95
BUILD=Debug
MSVC=g:\msvc22
MSVC16=g:\msvc15
MSTOOLS=g:\mstools
WIN=c:\windows
TARGET=sh
!ENDIF

# set defaults

FROM_MAKEFILE=makefile

# choices are HOST= [WIN95 WIN31 WINNT]
!IF "$(HOST)" == ""
HOST=WIN95
!ENDIF

!IF "$(BUILD)" != "Release"
DEBUG=ON
BUILD=Debug
!ENDIF

!IF "$(SRCDIR)" == ""
SRCDIR=g:\devo
!ENDIF
!IF "$(OUTDIR)" == ""
OUTDIR=c:\gs
!ENDIF
!IF "$(INTDIR)" == ""
INTDIR=c:\gs\tmp
!ENDIF


!IF "$(MSVC)" == ""
MSVC=c:\msvc22
!ENDIF
!IF "$(MSVC16)" == ""
MSVC16=c:\msvc15
!ENDIF
!IF "$(MSTOOLS)" == ""
MSTOOLS=c:\mstools
!ENDIF
!IF "$(WIN)" == ""
WIN=c:\windows
!ENDIF

MSWINDIR=$(SRCDIR)\gdb\mswin
EXE=$(OUTDIR)\gdb.exe
BSC=$(OUTDIR)\gui.bsc


!IF "(TARGET)" == "all"
MAKE_TARGET=sh m68k sparclet sparclit i386
!ELSEIF "$(TARGET)" == ""
MAKE_TARGET=sh
!ELSE
MAKE_TARGET=$(TARGET)
!ENDIF

all: common.mak serdll.mak $(MAKE_TARGET)

sh: sh.mak common.mak
	nmake -f sh.mak

m68k: m68k.mak common.mak
	nmake -f m68k.mak

sparclet: sparclet.mak common.mak
	nmake -f sparcletm68k.mak

sparclit: sparclit.mak common.mak
	nmake -f sparclit.mak

i386: i386.mak common.mak
	nmake -f i386.mak

clean:
!IF "$(MAKE_TARGET)" == "all"
	nmake -f sh.mak clean
	nmake -f m68k.mak clean
	nmake -f sparcletm68k.mak clean
	nmake -f sparclit.mak clean
	nmake -f i386.mak clean
!ELSE
	nmake -f $(MAKE_TARGET).mak clean
!ENDIF

