# makefile to create i386 target for wingdb
# from mswin directory, run:
#	nmake -f i386.mak
# change settings in config.mak as needed for your environment

# target specific extras for config.mak
TARGET=i386
TARGET_CFLAGS= -D TARGET_I386
TARGET_LFLAGS= 
!include "config.mak"

# target specific extras for common.mak
TARGET_DEPS= $(OUTDIR)\$(SIM).exe

TARGET_OBJS= \
	$(INTDIR)/aout32.obj \
	$(INTDIR)/cpu-i386.obj \
	$(INTDIR)/i386-dis.obj \
	$(INTDIR)/i386-tdep.obj \
	$(INTDIR)/i386aout.obj \
	$(INTDIR)/stab-syms.obj \
	$(INTDIR)/version.obj

TARGET_SBRS=  \
	$(INTDIR)/aout32.sbr \
	$(INTDIR)/cpu-i386.sbr \
	$(INTDIR)/i386-dis.sbr \
	$(INTDIR)/i386-tdep.sbr \
	$(INTDIR)/i386aout.sbr \
	$(INTDIR)/stab-syms.sbr \
	$(INTDIR)/version.sbr


!include "common.mak"


# simulator

TARGET_LIBLIB_OBJS= \
	$(INTDIR)\alloca.obj \
	$(INTDIR)\bcopy.obj 

#these top 2 were already separate...
TARGET_LIBBFD_OBJS=  \
	$(INTDIR)\ecoff.obj \
	$(INTDIR)\ecofflink.obj \
	$(INTDIR)\coff-i386.obj \
	$(INTDIR)\coffgen.obj

GENCODE=engine.c
!include "sim.mak"

$(INTDIR)\sim\run.obj : $(SRCDIR)\sim\common\run.c
$(INTDIR)\sim\callback.obj : $(SRCDIR)\gdb\callback.c
$(INTDIR)\sim\interp.obj : $(SRCDIR)\sim\$(TARGET)\interp.c $(SRCDIR)\gdb\mswin\prebuilt\$(TARGET)\engine.c

