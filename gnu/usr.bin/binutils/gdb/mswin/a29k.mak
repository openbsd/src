# makefile to create a29k target for wingdb
# from mswin directory, run:
#	nmake -f a29k.mak
# change settings in config.mak as needed for your environment

# target specific extras for config.mak
TARGET=a29k
TARGET_CFLAGS= -D TARGET_A29K -I $(SRCDIR)\gdb\29k-share\udi
TARGET_LFLAGS= 
!include "config.mak"

# target specific extras for common.mak
TARGET_DEPS= 

# where do these come from??

TARGET_OBJS= \
	$(INTDIR)/a29k-dis.obj \
	$(INTDIR)/a29k-tdep.obj \
	$(INTDIR)/coff-a29k.obj \
	$(INTDIR)/cpu-a29k.obj \
	$(INTDIR)/remote-udi.obj \
	$(INTDIR)/stab-syms.obj \
	$(INTDIR)/udi2go32.obj  \
	$(INTDIR)/udip2soc.obj  \
	$(INTDIR)/udr.obj \
	$(INTDIR)/version.obj

TARGET_SBRS=  \
	$(INTDIR)/a29k-dis.sbr \
	$(INTDIR)/a29k-tdep.sbr \
	$(INTDIR)/coff-a29k.sbr \
	$(INTDIR)/cpu-a29k.sbr \
	$(INTDIR)/remote-udi.sbr \
	$(INTDIR)/stab-syms.sbr \
	$(INTDIR)/udi2go32.sbr  \
	$(INTDIR)/udip2soc.sbr \
	$(INTDIR)/udr.sbr \
	$(INTDIR)/version.sbr


!include "common.mak"

{$(SRCDIR)\gdb\29k-share\udi}.c{$(INTDIR))}.obj:
   $(DECHO) $(CPP) $(GDB_CFLAGS) $(CFLAGS) $<
   $(CPP) @<<
$(GDB_CFLAGS) $(CFLAGS) $<
<<

$(INTDIR)/udi2go32.obj : $(SRCDIR)\gdb\29k-share\udi/udi2go32.c
$(INTDIR)/udip2soc.obj : $(SRCDIR)\gdb\29k-share\udi/udip2soc.c
