# makefile to create sparclite target for wingdb
# from mswin directory, run:
#	nmake -f sparclit.mak
# change settings in config.mak as needed for your environment

# target specific extras for config.mak
TARGET=sparclite
TARGET_CFLAGS= /D TARGET_SPARCLITE 
TARGET_LFLAGS= 
!include "config.mak"

# target specific extras for common.mak
TARGET_DEPS= 

TARGET_OBJS= \
	$(INTDIR)/aout32.obj \
	$(INTDIR)/coff-sparc.obj \
	$(INTDIR)/cpu-sparc.obj \
	$(INTDIR)/sparc-dis.obj \
	$(INTDIR)/sparc-opc.obj \
	$(INTDIR)/sparc-tdep.obj \
	$(INTDIR)/sparcl-tdep.obj \
	$(INTDIR)/stab-syms.obj \
	$(INTDIR)/sunos.obj \
	$(INTDIR)/version.obj

TARGET_SBRS=  \
	$(INTDIR)/aout32.sbr \
	$(INTDIR)/coff-sparc.sbr \
	$(INTDIR)/cpu-sparc.sbr \
	$(INTDIR)/sparc-dis.sbr \
	$(INTDIR)/sparc-opc.sbr \
	$(INTDIR)/sparc-tdep.sbr \
	$(INTDIR)/sparcl-tdep.sbr \
	$(INTDIR)/stab-syms.sbr \
	$(INTDIR)/sunos.sbr \
	$(INTDIR)/version.sbr


!include "common.mak"


