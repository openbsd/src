# makefile to create sparclet target for wingdb
# from mswin directory, run:
#	nmake -f sparclet.mak
# change settings in config.mak as needed for your environment

# target specific extras for config.mak
TARGET=sparclet
TARGET_CFLAGS= -D TARGET_SPARCLET 
TARGET_LFLAGS= 
TARGET_LFLAGS32 = $(MSVC)\lib\oldnames.lib 
!include "config.mak"

# target specific extras for common.mak
TARGET_DEPS=

TARGET_OBJS= \
	$(INTDIR)/coff-sparc.obj \
	$(INTDIR)/sparc-dis.obj \
	$(INTDIR)/sparc-tdep.obj \
	$(INTDIR)/sparclet-rom.obj \
	$(INTDIR)/cpu-sparc.obj \
	$(INTDIR)/aout0.obj \
	$(INTDIR)/aout32.obj \
	$(INTDIR)/sunos.obj \
	$(INTDIR)/stab-syms.obj \
	$(INTDIR)/sparc-opc.obj \
	$(INTDIR)/version.obj

TARGET_SBRS=  \
	$(INTDIR)/coff-sparc.sbr \
	$(INTDIR)/sparc-dis.sbr \
	$(INTDIR)/sparc-tdep.sbr \
	$(INTDIR)/sparclet-rom.sbr \
	$(INTDIR)/cpu-sparc.sbr \
	$(INTDIR)/aout0.sbr \
	$(INTDIR)/aout32.sbr \
	$(INTDIR)/sunos.sbr \
	$(INTDIR)/stab-syms.sbr \
	$(INTDIR)/sparc-opc.sbr \
	$(INTDIR)/version.sbr

!include "common.mak"

