
# makefile to create sh target for wingdb
# from mswin directory, run:
#	nmake -f sh.mak
# output files will be copied to OUTDIR (see settings below)

# target specific extras for config.mak

TARGET=sh
TARGET_LFLAGS32 = $(INTDIR)\wine7kpc.lib $(INTDIR)\winthunk.lib $(MSVC)\lib\thunk32.lib
TARGET_CFLAGS= -D TARGET_SH 
TARGET_LFLAGS= 
!include "config.mak"


# target specific extras for common.mak
TARGET_DEPS=$(OUTDIR)\wine7kpc.dll $(INTDIR)\wine7kpc.lib $(INTDIR)\winthunk.lib $(OUTDIR)\$(SIM).exe


TARGET_OBJS= \
	$(INTDIR)/win-e7kpc.obj \
	$(INTDIR)/sh-tdep.obj \
	$(INTDIR)/interp.obj \
	$(INTDIR)/table.obj \
	$(INTDIR)/sh-dis.obj \
	$(INTDIR)/cpu-sh.obj \
	$(INTDIR)/coff-sh.obj \
        $(INTDIR)/stab-syms.obj \
	$(INTDIR)/remote-sim.obj \
	$(INTDIR)/version.obj \
	$(INTDIR)/sh3-rom.obj \
	$(INTDIR)/remote-e7000.obj \
	$(INTDIR)/ser-e7kpc.obj

TARGET_SBRS= \
	$(INTDIR)/win-e7kpc.sbr \
	$(INTDIR)/sh-tdep.sbr \
	$(INTDIR)/interp.sbr \
	$(INTDIR)/table.sbr \
	$(INTDIR)/sh-dis.sbr \
	$(INTDIR)/cpu-sh.sbr \
	$(INTDIR)/coff-sh.sbr \
        $(INTDIR)/stab-syms.sbr \
	$(INTDIR)/remote-sim.sbr \
	$(INTDIR)/version.sbr \
	$(INTDIR)/sh3-rom.sbr \
	$(INTDIR)/remote-e7000.sbr \
	$(INTDIR)/ser-e7kpc.sbr

!include "common.mak"


# additional rules


$(INTDIR)\win-e7kpc16.obj: win-e7kpc16.c win-e7kpc.h msvcenv16 
	$(CC16) $(GDB_CFLAGS16) $(CFLAGS16) win-e7kpc16.c
	@set INCLUDE=$(MSVC)\INCLUDE;$(MSVC)\MFC\INCLUDE;
        @set LIB=$(MSVC)\LIB;$(MSVC)\MFC\LIB;
        @set PATH=$(MSVC)\BIN;$(MSVC)\BIN\WIN95;$(WIN);$(WIN)\COMMAND;
	@echo FIXME!! $(SETMSVC) bad...

$(INTDIR)\win-e7kpc.obj: win-e7kpc.c win-e7kpc.h 
	$(CC) @<<
  $(GDB_CFLAGS) $(CFLAGS) win-e7kpc.c
<<

$(INTDIR)\win-e7kpc-test.obj: win-e7kpc.c win-e7kpc.h 
	$(CC) @<<
  $(GDB_CFLAGS) $(CFLAGS) -D "STAND_ALONE" win-e7kpc.c /Fo"$(INTDIR)\win-e7kpc-test.obj"
<<

$(OUTDIR)\wine7kpc.dll: $(INTDIR)\win-e7kpc16.obj wine7kpc.def msvcenv16
	$(LINK16) $(GDB_LFLAGS16) $(LFLAGS16) $(INTDIR)\win-e7kpc16.obj, $(OUTDIR)\wine7kpc.dll, , $(TARGET_LIBS_DLL16) , wine7kpc.def
	@set INCLUDE=$(MSVC)\INCLUDE;$(MSVC)\MFC\INCLUDE;
        @set LIB=$(MSVC)\LIB;$(MSVC)\MFC\LIB;
        @set PATH=$(MSVC)\BIN;$(MSVC)\BIN\WIN95;$(WIN);$(WIN)\COMMAND;
	@echo FIXME!! $(SETMSVC) bad...

$(INTDIR)\wine7kpc.lib: wine7kpc.def 
	$(IMPLIB) /MACHINE:IX86 /DEF:wine7kpc.def -out:$(INTDIR)\wine7kpc.lib

$(INTDIR)\winthunk.lib: winthunk.def 
	$(IMPLIB) /MACHINE:IX86 /DEF:winthunk.DEF -out:$(INTDIR)\winthunk.lib

# test program for e7000 pc
$(OUTDIR)\wine7kpc.exe: $(INTDIR)\win-e7kpc-test.obj $(INTDIR)\wine7kpc.lib $(INTDIR)\winthunk.lib 
	$(LINK32) @<<
  $(GDB_LFLAGS) $(LFLAGS) $(INTDIR)\win-e7kpc-test.obj /out:$(OUTDIR)\wine7kpc.exe $(INTDIR)\wine7kpc.lib $(INTDIR)\winthunk.lib $(MSVC)\lib\thunk32.lib $(LIBS) -subsystem:console -entry:mainCRTStartup $(MSVC)/lib/kernel32.lib $(MSVC)/lib/user32.lib 
<<

# simulator

TARGET_LIBLIB_OBJS= \
        $(INTDIR)\bcopy.obj

#these top 2 were already separate...
TARGET_LIBBFD_OBJS=  \
	$(INTDIR)/table.obj \
	$(INTDIR)/cpu-sh.obj \
	$(INTDIR)/coff-sh.obj \
	$(INTDIR)/cofflink.obj \
	$(INTDIR)/coffgen.obj 

TARGET_SIM_OBJS= \
	$(INTDIR)/stubs.obj


GENCODE=code.c
!include "sim.mak"

$(INTDIR)\sim\run.obj : $(SRCDIR)\sim\common\run.c
$(INTDIR)\sim\callback.obj : $(SRCDIR)\gdb\callback.c
$(INTDIR)\sim\interp.obj : $(SRCDIR)\sim\$(TARGET)\interp.c $(SRCDIR)\gdb\mswin\prebuilt\$(TARGET)\$(GENCODE)

