# simulator
# can set TARGET_SIM_OBJS

# additional rules

$(SRCDIR)\gdb\mswin\prebuilt\code.c : $(SRCDIR)\gdb\gencode
	gencode -x > $(SRCDIR)\gdb\mswin\prebuilt\code.c

$(SRCDIR)\gdb\mswin\prebuilt\code.c : $(SRCDIR)\gdb\gencode.c 
    $(CFLAGS) @<<
  $(SRCDIR)\gdb\gencode.c /out=$(SRCDIR)\gdb\mswin\prebuilt\code.c 
<<

SIM_OBJS=$(TARGET_SIM_OBJS) $(INTDIR)\sim\interp.obj $(INTDIR)\sim\run.obj \
	$(INTDIR)\sim\callback.obj
SIM_LIBS=$(INTDIR)\libbfd.lib $(INTDIR)\libiberty.lib $(INTDIR)\libbfd.lib $(INTDIR)\libiberty.lib $(INTDIR)\libbfd.lib 

#currently callback.c is the only src which tests INSIDE_SIMULATOR
{$(SRCDIR)/gdb}.c{$(INTDIR)/sim}.obj: 
	$(CPP) @<<
$(SIM_CFLAGS) $(CFLAGS) $<
<<

{$(SRCDIR)/sim/common}.c{$(INTDIR)/sim}.obj: 
	$(DECHO) $(SIM_CFLAGS) $(CFLAGS) $<
	$(CPP) @<<
$(SIM_CFLAGS) $(CFLAGS) $<
<<

{$(SRCDIR)/sim/$(TARGET)}.c{$(INTDIR)/sim}.obj: 
	$(DECHO) $(SIM_CFLAGS) $(CFLAGS) $<
	$(CPP) @<<
$(SIM_CFLAGS) $(CFLAGS) $<
<<

$(INTDIR)\sim\nul : $(INTDIR)\nul
	mkdir $(INTDIR)\sim

LIBLIB_OBJS= \
	$(TARGET_LIBLIB_OBJS) \
	$(INTDIR)\argv.obj \
	$(INTDIR)\basename.obj \
	$(INTDIR)\concat.obj \
	$(INTDIR)\cplus-dem.obj \
	$(INTDIR)\fdmatch.obj \
	$(INTDIR)\getopt.obj \
	$(INTDIR)\getopt1.obj \
	$(INTDIR)\getruntime.obj \
	$(INTDIR)\hex.obj \
	$(INTDIR)\floatformat.obj \
	$(INTDIR)\obstack.obj \
	$(INTDIR)\spaces.obj \
	$(INTDIR)\vasprintf.obj \
	$(INTDIR)\xatexit.obj \
	$(INTDIR)\xexit.obj \
	$(INTDIR)\xmalloc.obj \
	$(INTDIR)\xstrdup.obj \
	$(INTDIR)\xstrerror.obj \
	$(INTDIR)\atexit.obj \
	$(INTDIR)\memmove.obj \


LIBBFD_OBJS= \
	$(TARGET_LIBBFD_OBJS) \
	$(INTDIR)\archive.obj \
	$(INTDIR)\archures.obj \
	$(INTDIR)\bfd.obj \
	$(INTDIR)\binary.obj \
	$(INTDIR)\cache.obj \
	$(INTDIR)\corefile.obj \
	$(INTDIR)\format.obj \
	$(INTDIR)\hash.obj \
	$(INTDIR)\ihex.obj \
	$(INTDIR)\init.obj \
	$(INTDIR)\libbfd.obj \
	$(INTDIR)\linker.obj \
	$(INTDIR)\opncls.obj \
	$(INTDIR)\reloc.obj \
	$(INTDIR)\section.obj \
	$(INTDIR)\srec.obj \
	$(INTDIR)\stab-syms.obj \
	$(INTDIR)\stabs.obj \
	$(INTDIR)\syms.obj \
	$(INTDIR)\targets.obj \
	$(INTDIR)\tekhex.obj 


LIBSIM_OBJS=$(INTDIR)\sim\interp.obj

# from sh sim
#	$(INTDIR)/coff-sh.obj 
#	$(INTDIR)/cpu-sh.obj 

# may want to add /debugtype:coff, ...
$(INTDIR)\libbfd.lib: $(LIBBFD_OBJS)
	@echo ======= building $(INTDIR)\libbfd.lib ======
	$(DECHO) /OUT:$(INTDIR)\libbfd.lib $(LIBBFD_OBJS)
	$(AR) @<<
   /OUT:$(INTDIR)\libbfd.lib $(LIBBFD_OBJS)
<<

$(INTDIR)\libiberty.lib: $(LIBLIB_OBJS)
	@echo ======= building $(INTDIR)\libiberty.lib ======
	$(DECHO) /OUT:$(INTDIR)\libiberty.lib $(LIBLIB_OBJS)
	$(AR) @<<
   /OUT:$(INTDIR)\libiberty.lib $(LIBLIB_OBJS)
<<

$(INTDIR)\sim\run.obj : $(SRCDIR)\sim\common\run.c
$(INTDIR)\sim\callback.obj : $(SRCDIR)\gdb\callback.c
$(INTDIR)\sim\interp.obj : $(SRCDIR)\sim\$(TARGET)\interp.c $(SRCDIR)\gdb\mswin\prebuilt\$(TARGET)\$(GENCODE)

$(INTDIR)/compile.obj : $(SRCDIR)/sim/$(TARGET)/compile.c
$(INTDIR)/table.obj : $(SRCDIR)/gdb/mswin/prebuilt/$(TARGET)/table.c
$(INTDIR)/interp.obj : $(SRCDIR)/sim/$(TARGET)/interp.c $(SRCDIR)/gdb/mswin/prebuilt/$(TARGET)/$(GENCODE)

$(INTDIR)\libsim.lib : $(LIBSIM_OBJS)
	@echo ======= building $(INTDIR)\libsim.lib ======
	$(DECHO) $(AR) $(ARFLAGS) $(INTDIR)\libsim.lib $(LIBSIM_OBJS)
	$(AR) $(ARFLAGS) $(INTDIR)\libsim.lib $(LIBSIM_OBJS)
	$(RANLIB) $(INTDIR)\libsim.lib 

$(OUTDIR)\$(SIM).exe : $(OUTDIR) $(INTDIR) $(INTDIR)/sim/nul $(SIM_OBJS) $(SIM_LIBS) 
	@echo ======= linking $(OUTDIR)\$(SIM).exe ======
    $(LINK32) @<<
  $(SIM_LFLAGS) $(LFLAGS) $(SIM_OBJS) $(SIM_LIBS) $(TARGET_LFLAGS) /OUT:$(OUTDIR)\$(SIM).exe /SUBSYSTEM:console 
<<

