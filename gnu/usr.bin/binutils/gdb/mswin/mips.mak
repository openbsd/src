# makefile to create mmips target for wingdb
# from mswin directory, run:
#	nmake -f mips.mak
# change settings in config.mak as needed for your environment

# target specific extras for config.mak
TARGET=mips
TARGET_CFLAGS= -D TARGET_MIPS 
TARGET_LFLAGS= 
!include "config.mak"

# target specific extras for common.mak
TARGET_DEPS= $(OUTDIR)\$(SIM).exe

TARGET_OBJS= \
	$(INTDIR)/ecoff.obj \
	$(INTDIR)/ecofflink.obj \
	$(INTDIR)/coff-mips.obj \
	$(INTDIR)/elf.obj \
	$(INTDIR)/elflink.obj \
	$(INTDIR)/elf32-mips.obj \
	$(INTDIR)/elf32-gen.obj \
	$(INTDIR)/elf32.obj \
	$(INTDIR)/elf64.obj \
	$(INTDIR)/elf64-gen.obj \
	$(INTDIR)/elf64-mips.obj \
        $(INTDIR)/stab-syms.obj \
	$(INTDIR)/remote-mips.obj \
	$(INTDIR)/remote-sim.obj \
	$(INTDIR)/cpu-mips.obj \
	$(INTDIR)/mips-tdep.obj \
	$(INTDIR)/mips-opc.obj \
	$(INTDIR)/mips-dis.obj \
	$(INTDIR)/version.obj \
	$(INTDIR)/interp.obj

TARGET_SBRS=  \
	$(INTDIR)/elf.sbr \
	$(INTDIR)/elflink.sbr \
	$(INTDIR)/elf32-mips.sbr \
	$(INTDIR)/elf32-gen.sbr \
	$(INTDIR)/elf32.sbr \
	$(INTDIR)/elf64.sbr \
	$(INTDIR)/elf64-gen.sbr \
	$(INTDIR)/elf64-mips.sbr \
        $(INTDIR)/stab-syms.sbr \
	$(INTDIR)/remote-mips.sbr \
	$(INTDIR)/remote-sim.sbr \
	$(INTDIR)/cpu-mips.sbr \
	$(INTDIR)/mips-tdep.sbr \
	$(INTDIR)/mips-opc.sbr \
	$(INTDIR)/mips-dis.sbr \
	$(INTDIR)/version.sbr \
	$(INTDIR)/interp.sbr


!include "common.mak"


# simulator

TARGET_LIBLIB_OBJS= \
	$(INTDIR)\alloca.obj \
	$(INTDIR)\bcopy.obj 

#these top 2 were already separate...
TARGET_LIBBFD_OBJS=  \
	$(INTDIR)\ecoff.obj \
	$(INTDIR)\ecofflink.obj \
	$(INTDIR)\coff-mips.obj \
	$(INTDIR)\coffgen.obj \
	$(INTDIR)\cpu-mips.obj \
	$(INTDIR)\elf.obj \
	$(INTDIR)\elflink.obj \
	$(INTDIR)\elf32-mips.obj \
	$(INTDIR)\elf32-gen.obj \
	$(INTDIR)\elf32.obj \
	$(INTDIR)\elf64-mips.obj \
	$(INTDIR)\elf64-gen.obj \
	$(INTDIR)\elf64.obj 

GENCODE=engine.c
!include "sim.mak"

$(INTDIR)\sim\run.obj : $(SRCDIR)\sim\common\run.c
$(INTDIR)\sim\callback.obj : $(SRCDIR)\gdb\callback.c
$(INTDIR)\sim\interp.obj : $(SRCDIR)\sim\$(TARGET)\interp.c $(SRCDIR)\gdb\mswin\prebuilt\$(TARGET)\engine.c

