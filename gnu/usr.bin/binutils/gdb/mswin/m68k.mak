# makefile to create m68k target for wingdb
# from mswin directory, run:
#	nmake -f m68k.mak
# change settings in config.mak as needed for your environment

# target specific extras for config.mak
TARGET=m68k
TARGET_CFLAGS= -D TARGET_M68K 
TARGET_LFLAGS= 
!include "config.mak"

# target specific extras for common.mak
TARGET_DEPS= 

TARGET_OBJS= \
	$(INTDIR)/aout0.obj \
	$(INTDIR)/aout32.obj \
	$(INTDIR)/coff-m68k.obj \
	$(INTDIR)/cpu-m68k.obj \
	$(INTDIR)/cpu32bug-rom.obj \
	$(INTDIR)/m68k-dis.obj \
	$(INTDIR)/m68k-opc.obj \
	$(INTDIR)/m68k-tdep.obj \
	$(INTDIR)/remote-est.obj \
	$(INTDIR)/rom68k-rom.obj \
	$(INTDIR)/stab-syms.obj \
	$(INTDIR)/version.obj

TARGET_SBRS=  \
	$(INTDIR)/aout0.sbr \
	$(INTDIR)/aout32.sbr \
	$(INTDIR)/coff-m68k.sbr \
	$(INTDIR)/cpu-m68k.sbr \
	$(INTDIR)/cpu32bug-rom.sbr \
	$(INTDIR)/m68k-dis.sbr \
	$(INTDIR)/m68k-opc.sbr \
	$(INTDIR)/m68k-tdep.sbr \
	$(INTDIR)/remote-est.sbr \
	$(INTDIR)/rom68k-rom.sbr \
	$(INTDIR)/stab-syms.sbr \
	$(INTDIR)/version.sbr


!include "common.mak"


