# makefile to create h8300 target for wingdb
# from mswin directory, run:
#	nmake -f h8300.mak
# change settings in config.mak as needed for your environment

# target specific extras for config.mak
TARGET=H8300
TARGET_CFLAGS= -D TARGET_H8300 
TARGET_LFLAGS= 
TARGET_LFLAGS32 = $(INTDIR)\wine7kpc.lib $(INTDIR)\winthunk.lib $(MSVC)\lib\thunk32.lib
!include "config.mak"

# target specific extras for common.mak
TARGET_DEPS=$(OUTDIR)\wine7kpc.dll $(INTDIR)\wine7kpc.lib $(INTDIR)\winthunk.lib
!IF "1" == "0"
TARGET_DEPS= $(TARGET_DEPS) $(OUTDIR)\$(SIM).exe
!ENDIF


TARGET_OBJS= \
	$(INTDIR)/win-e7kpc.obj \
	$(INTDIR)/coff-h8300.obj \
	$(INTDIR)/compile.obj \
	$(INTDIR)/cpu-h8300.obj \
	$(INTDIR)/h8300-dis.obj \
	$(INTDIR)/h8300-tdep.obj \
	$(INTDIR)/reloc16.obj \
	$(INTDIR)/remote-e7000.obj \
	$(INTDIR)/remote-hms.obj \
	$(INTDIR)/remote-sim.obj \
	$(INTDIR)/ser-e7kpc.obj \
	$(INTDIR)/stab-syms.obj \
	$(INTDIR)/version.obj 

TARGET_SBRS=  \
	$(INTDIR)/win-e7kpc.sbr \
	$(INTDIR)/coff-h8300.sbr \
	$(INTDIR)/compile.sbr \
	$(INTDIR)/cpu-h8300.sbr \
	$(INTDIR)/h8300-dis.sbr \
	$(INTDIR)/h8300-tdep.sbr \
	$(INTDIR)/reloc16.sbr \
	$(INTDIR)/remote-e7000.sbr \
	$(INTDIR)/remote-hms.sbr \
	$(INTDIR)/remote-sim.sbr \
	$(INTDIR)/ser-e7kpc.sbr \
	$(INTDIR)/stab-syms.sbr \
	$(INTDIR)/version.sbr 

!include "common.mak"



# additional rules


$(INTDIR)\win-e7kpc16.obj: win-e7kpc16.c win-e7kpc.h msvcenv16 
	$(CC16) $(GDB_CFLAGS16) $(CFLAGS16) win-e7kpc16.c
	@set INCLUDE=$(INCLUDE32)
        @set LIB=$(LIB32)
        @set PATH=$(PATH32)

$(INTDIR)\win-e7kpc.obj: win-e7kpc.c win-e7kpc.h 
	$(CC) @<<
  $(CFLAGS) $(GDB_CFLAGS) win-e7kpc.c
<<

$(INTDIR)\win-e7kpc-test.obj: win-e7kpc.c win-e7kpc.h 
	$(CC) @<<
  $(CFLAGS) -D "STAND_ALONE" win-e7kpc.c /Fo"$(INTDIR)\win-e7kpc-test.obj"
<<

$(OUTDIR)\wine7kpc.dll: $(INTDIR)\win-e7kpc16.obj wine7kpc.def msvcenv16
	$(LINK16) $(GDB_LFLAGS16) $(LFLAGS16) $(INTDIR)\win-e7kpc16.obj, $(OUTDIR)\wine7kpc.dll, , $(TARGET_LIBS_DLL16) , wine7kpc.def
	@set INCLUDE=$(INCLUDE32)
        @set LIB=$(LIB32)
        @set PATH=$(PATH32)

$(INTDIR)\wine7kpc.lib: wine7kpc.def 
	$(IMPLIB) /MACHINE:IX86 /DEF:wine7kpc.def -out:$(INTDIR)\wine7kpc.lib

$(INTDIR)\winthunk.lib: winthunk.def 
	$(IMPLIB) /MACHINE:IX86 /DEF:winthunk.DEF -out:$(INTDIR)\winthunk.lib

# test program for e7000 pc
$(OUTDIR)\wine7kpc.exe: $(INTDIR)\win-e7kpc-test.obj $(INTDIR)\wine7kpc.lib $(INTDIR)\winthunk.lib 
	$(LINK32) @<<
  $(LFLAGS32) $(INTDIR)\win-e7kpc-test.obj /out:$(OUTDIR)\wine7kpc.exe $(INTDIR)\wine7kpc.lib $(INTDIR)\winthunk.lib $(MSVC)\lib\thunk32.lib $(LIBS) -subsystem:console -entry:mainCRTStartup $(MSVC)/lib/kernel32.lib $(MSVC)/lib/user32.lib 
<<

# simulator
!IF "1" == "0"

TARGET_LIBLIB_OBJS= \
	$(INTDIR)\alloca.obj \
	$(INTDIR)\bcopy.obj 

#these top 2 were already separate...
TARGET_LIBBFD_OBJS=  \
	$(INTDIR)\ecoff.obj \
	$(INTDIR)\ecofflink.obj \
	$(INTDIR)\coff-h8300.obj \
	$(INTDIR)\coffgen.obj

GENCODE=engine.c
!include "sim.mak"

$(INTDIR)\sim\run.obj : $(SRCDIR)\sim\common\run.c
$(INTDIR)\sim\callback.obj : $(SRCDIR)\gdb\callback.c
$(INTDIR)\sim\interp.obj : $(SRCDIR)\sim\$(TARGET)\interp.c $(SRCDIR)\gdb\mswin\prebuilt\$(TARGET)\engine.c

!ENDIF
