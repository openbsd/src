# makefile to create gm target for wingdb
# from mswin directory, run:
#	nmake -f gm.mak
# change settings in config.mak as needed for your environment

# target specific extras for config.mak
TARGET=gm
TARGET_CFLAGS= /D TARGET_MIPS /D GENERAL_MAGIC 
TARGET_LFLAGS= 
!include "config.mak"

# target specific extras for common.mak
TARGET_DEPS= 

#make file did have this extra objs, but must have meant cexptab...
#	$(INTDIR)/chexptab.obj 

# what's this??
#	$(INTDIR)/regex.obj 
#	$(INTDIR)/core.obj 
#	$(INTDIR)/win32.obj
#	$(INTDIR)/strsignal.obj

TARGET_OBJS= \
	$(INTDIR)/coff-mips.obj \
	$(INTDIR)/cpu-mips.obj \
	$(INTDIR)/ecoff.obj \
	$(INTDIR)/ecofflink.obj \
	$(INTDIR)/elf.obj \
	$(INTDIR)/elf32-mips.obj \
	$(INTDIR)/elf32.obj \
	$(INTDIR)/elflink.obj \
	$(INTDIR)/gmagic.obj \
	$(INTDIR)/mips-dis.obj \
	$(INTDIR)/mips-opc.obj \
	$(INTDIR)/mips-tdep.obj \
	$(INTDIR)/remote-mips.obj \
	$(INTDIR)/strerror.obj \
	$(INTDIR)/version.obj 

TARGET_SBRS=  \
	$(INTDIR)/coff-mips.sbr \
	$(INTDIR)/cpu-mips.sbr \
	$(INTDIR)/ecoff.sbr \
	$(INTDIR)/ecofflink.sbr \
	$(INTDIR)/elf.sbr \
	$(INTDIR)/elf32-mips.sbr \
	$(INTDIR)/elf32.sbr \
	$(INTDIR)/elflink.sbr \
	$(INTDIR)/gmagic.sbr \
	$(INTDIR)/mips-dis.sbr \
	$(INTDIR)/mips-opc.sbr \
	$(INTDIR)/mips-tdep.sbr \
	$(INTDIR)/remote-mips.sbr \
	$(INTDIR)/strerror.sbr \
	$(INTDIR)/version.sbr 


!include "common.mak"

