# common.mak - must be included by various target-specific wingdb makefiles.

# 	paths are relative to mswin directory.



# Rules for compiling source from various directories

{$(MSWINDIR)}.c{$(INTDIR)}.obj: 
   $(DECHO) $(CPP) $(GDB_CFLAGS) $(CFLAGS) $<
   $(CPP) @<<
$(GDB_CFLAGS) $(CFLAGS) $<
<<  

{$(MSWINDIR)}.cpp{$(INTDIR)}.obj: 
   $(DECHO) $(CPP) $(GDB_CFLAGS) $(CFLAGS) $<
   $(CPP) @<<
$(GDB_CFLAGS) $(CFLAGS) $<
<<  

{$(MSWINDIR)}.cxx{$(INTDIR))}.obj: 
   $(DECHO) $(CPP) $(GDB_CFLAGS) $(CFLAGS) $<
   $(CPP) @<<
$(GDB_CFLAGS) $(CFLAGS) $<
<<  

{$(SRCDIR)/bfd}.c{$(INTDIR)}.obj:
   $(DECHO) $(CPP) $(GDB_CFLAGS) $(CFLAGS) $<
   $(CPP) @<<
$(GDB_CFLAGS) $(CFLAGS) $<
<<  

{$(SRCDIR)/libiberty}.c{$(INTDIR)}.obj: 
   $(DECHO) $(CPP) $(GDB_CFLAGS) $(CFLAGS) $<
   $(CPP) @<<
$(GDB_CFLAGS) $(CFLAGS) $<
<<  

{$(SRCDIR)/readline}.c{$(INTDIR)}.obj: 
   $(DECHO) $(CPP) $(GDB_CFLAGS) $(CFLAGS) $<
   $(CPP) @<<
$(GDB_CFLAGS) $(CFLAGS) $<
<<  

{$(SRCDIR)/opcodes}.c{$(INTDIR)}.obj: 
   $(DECHO) $(CPP) $(GDB_CFLAGS) $(CFLAGS) $<
   $(CPP) @<<
$(GDB_CFLAGS) $(CFLAGS) $<
<<  

{$(SRCDIR)/gdb}.c{$(INTDIR)}.obj: 
   $(DECHO) $(CPP) $(GDB_CFLAGS) $(CFLAGS) $<
   $(CPP) @<<
$(GDB_CFLAGS) $(CFLAGS) $<
<<  

{$(SRCDIR)/gdb/mswin/prebuilt/gdb}.c{$(INTDIR)}.obj: 
   $(DECHO) $(CPP) $(GDB_CFLAGS) $(CFLAGS) $<
   $(CPP) @<<
$(GDB_CFLAGS) $(CFLAGS) $<
<<  

#targets:
# a29k h8300 i386 m68k mips sh sparclite sparclet

{$(SRCDIR)/gdb/mswin/prebuilt/$(TARGET)}.c{$(INTDIR)}.obj: 
   $(DECHO) $(CPP) $(GDB_CFLAGS) $(CFLAGS) $<
   $(CPP) @<<
$(GDB_CFLAGS) $(CFLAGS) $<
<<  

{$(SRCDIR)/sim/$(TARGET)}.c{$(INTDIR)}.obj: 
   $(DECHO) $(CPP) $(GDB_CFLAGS) $(CFLAGS) $<
   $(CPP) @<<
$(GDB_CFLAGS) $(CFLAGS) $<
<<  


# default rule is to build gdb

# TARGET_DEPS are assumed to be dependancies for EXE
GDB_DEPS=$(OUTDIR)\serdll32.dll $(EXE) $(BSC)

all_gdb : msvcenv $(OUTDIR) $(INTDIR) $(SRCDIR) $(MSWINDIR) $(CPP) $(LINK32) addmstools $(GDB_DEPS)

exe: $(EXE)
sim: $(OUTDIR)\$(SIM).exe
nms: $(NMS)
bsc: $(BSC)

# don't use bats cuz they can cause us to run out of env space
SETMSVC= $(MSVC)\bin\vcvars32 x86
SETMSVC16= $(MSVC16)\bin\msvcvars

msvcenv :: $(MSVC32)\nul
	@set INCLUDE=$(INCLUDE32)
	@set LIB=$(LIB32)
	@set PATH=$(PATH32)
	@echo Setting env for $(MSVC32)
	$(DSET)
    	@if not exist $(MSVC)\bin\nul echo "MSVC not found; check MSVC setting in makefile"

msvcenv16 :: $(MSVC16)\nul
	@set INCLUDE=$(INCLUDE16)
	@set LIB=$(LIB16)
	@set PATH=$(PATH16)
	@echo Setting env for $(MSVC16)
	$(DSET)
    	@if not exist $(MSVC16)\bin\nul echo "MSVC16 not found; check MSVC16 setting in makefile"

addmstools :: $(MSTOOLS)\nul
	@set LIB=%LIB%;$(MSTOOLS)\lib;
	@set INCLUDE=%INCLUDE%;$(MSTOOLS)\include;
	@set PATH=%PATH%;$(MSTOOLS)\bin;
	@echo Adding env for $(MSTOOLS) 
	$(DSET)
    	@if not exist $(MSTOOLS)\bin\nul echo "MSTOOLS" not found; check MSTOOLS setting in makefile"

	
$(CPP) :: 
    @if not exist $(CPP) echo "MSVC compiler $(CPP) not found"
$(CC) :: 
    @if not exist $(CC) echo "MSVC compiler $(CC) not found"
$(LINK32) :: 
    @if not exist $(LINK32) echo "MSVC linker $(LINK32) not found"
$(AR) :: 
    @if not exist $(AR) echo "MSVC librarian $(AR) not found"
$(OUTDIR) :: 
    @if not exist $(OUTDIR)\nul mkdir $(OUTDIR)
$(INTDIR) :: 
    @if not exist $(INTDIR)\nul mkdir $(INTDIR)


# build gdb 

#	$(INTDIR)/$(GDB).res 
EXTRA_DEPS = \
	$(INTDIR)/gui.res  \
	$(OUTDIR)\serdll32.lib

COMMON_OBJS= \
	$(INTDIR)/gui.obj \
	$(INTDIR)/mainfrm.obj \
	$(INTDIR)/stdafx.obj \
	$(INTDIR)/aboutbox.obj \
	$(INTDIR)/iface.obj \
	$(INTDIR)/fontinfo.obj \
	$(INTDIR)/regview.obj \
	$(INTDIR)/regdoc.obj \
	$(INTDIR)/bptdoc.obj \
	$(INTDIR)/colinfo.obj \
	$(INTDIR)/gdbwrap.obj \
	$(INTDIR)/srcbrows.obj \
	$(INTDIR)/scview.obj \
	$(INTDIR)/framevie.obj \
	$(INTDIR)/browserl.obj \
	$(INTDIR)/browserf.obj \
	$(INTDIR)/gdbdoc.obj \
	$(INTDIR)/flash.obj \
	$(INTDIR)/stubs.obj \
	$(INTDIR)/gdbwin.obj \
	$(INTDIR)/gdbwinxx.obj \
	$(INTDIR)/initfake.obj \
	$(INTDIR)/infoframe.obj \
	$(INTDIR)/ginfodoc.obj \
	$(INTDIR)/srcwin.obj \
	$(INTDIR)/srcd.obj \
	$(INTDIR)/transbmp.obj \
	$(INTDIR)/expwin.obj \
	$(INTDIR)/fsplit.obj \
	$(INTDIR)/srcsel.obj \
	$(INTDIR)/props.obj \
	$(INTDIR)/dirpkr.obj \
	$(INTDIR)/srcb.obj \
	$(INTDIR)/mem.obj \
	$(INTDIR)/bfdcore.obj \
	$(INTDIR)/change.obj \
	$(INTDIR)/frameview.obj \
	$(INTDIR)/log.obj \
	$(INTDIR)/mini.obj \
	$(INTDIR)/option.obj \
	$(INTDIR)/ser-win32s.obj \
	$(INTDIR)/alloca.obj \
	$(INTDIR)/argv.obj \
	$(INTDIR)/bcmp.obj \
	$(INTDIR)/bzero.obj \
	$(INTDIR)/obstack.obj \
	$(INTDIR)/random.obj \
	$(INTDIR)/rindex.obj \
	$(INTDIR)/spaces.obj \
	$(INTDIR)/bcopy.obj \
	$(INTDIR)/concat.obj \
	$(INTDIR)/strtod.obj \
	$(INTDIR)/cplus-dem.obj \
	$(INTDIR)/vprintf.obj \
	$(INTDIR)/tmpnam.obj \
	$(INTDIR)/vasprintf.obj \
	$(INTDIR)/strdup.obj \
	$(INTDIR)/getopt1.obj \
	$(INTDIR)/insque.obj \
	$(INTDIR)/getopt.obj \
	$(INTDIR)/hex.obj \
	$(INTDIR)/getruntime.obj \
	$(INTDIR)/floatformat.obj \
	$(INTDIR)/strcasecmp.obj \
	$(INTDIR)/basename.obj \
	$(INTDIR)/dis-buf.obj \
	$(INTDIR)/disassemble.obj \
	$(INTDIR)/readline.obj \
	$(INTDIR)/search.obj \
	$(INTDIR)/signals.obj \
	$(INTDIR)/keymaps.obj \
	$(INTDIR)/funmap.obj \
	$(INTDIR)/isearch.obj \
	$(INTDIR)/display.obj \
	$(INTDIR)/parens.obj \
	$(INTDIR)/bind.obj \
	$(INTDIR)/rltty.obj \
	$(INTDIR)/complete.obj \
	$(INTDIR)/history.obj \
	$(INTDIR)/archive.obj \
	$(INTDIR)/archures.obj \
	$(INTDIR)/bfd.obj \
	$(INTDIR)/binary.obj \
	$(INTDIR)/cache.obj \
	$(INTDIR)/coffgen.obj \
	$(INTDIR)/cofflink.obj \
	$(INTDIR)/filemode.obj \
	$(INTDIR)/format.obj \
	$(INTDIR)/hash.obj \
	$(INTDIR)/init.obj \
	$(INTDIR)/libbfd.obj \
	$(INTDIR)/linker.obj \
	$(INTDIR)/opncls.obj \
	$(INTDIR)/reloc.obj \
	$(INTDIR)/section.obj \
	$(INTDIR)/srec.obj \
	$(INTDIR)/syms.obj \
	$(INTDIR)/targets.obj \
	$(INTDIR)/bpt.obj \
	$(INTDIR)/tekhex.obj \
	$(INTDIR)/versados.obj \
	$(INTDIR)/ihex.obj \
	$(INTDIR)/annotate.obj \
	$(INTDIR)/blockframe.obj \
	$(INTDIR)/breakpoint.obj \
	$(INTDIR)/buildsym.obj \
	$(INTDIR)/c-lang.obj \
	$(INTDIR)/c-typeprint.obj \
	$(INTDIR)/c-valprint.obj \
	$(INTDIR)/cexptab.obj \
	$(INTDIR)/ch-lang.obj \
	$(INTDIR)/ch-typeprint.obj \
	$(INTDIR)/ch-valprint.obj \
	$(INTDIR)/coffread.obj \
	$(INTDIR)/command.obj \
	$(INTDIR)/complaints.obj \
	$(INTDIR)/copying.obj \
	$(INTDIR)/corefile.obj \
	$(INTDIR)/cp-valprint.obj \
	$(INTDIR)/dbxread.obj \
	$(INTDIR)/dcache.obj \
	$(INTDIR)/demangle.obj \
	$(INTDIR)/dwarfread.obj \
	$(INTDIR)/dwarf2read.obj \
	$(INTDIR)/elfread.obj \
	$(INTDIR)/environ.obj \
	$(INTDIR)/eval.obj \
	$(INTDIR)/exec.obj \
	$(INTDIR)/expprint.obj \
	$(INTDIR)/f-lang.obj \
	$(INTDIR)/f-typeprint.obj \
	$(INTDIR)/f-valprint.obj \
	$(INTDIR)/fexptab.obj \
	$(INTDIR)/findvar.obj \
	$(INTDIR)/fork-child.obj \
	$(INTDIR)/gdbtypes.obj \
	$(INTDIR)/infcmd.obj \
	$(INTDIR)/inflow.obj \
	$(INTDIR)/infrun.obj \
	$(INTDIR)/language.obj \
	$(INTDIR)/m2-lang.obj \
	$(INTDIR)/m2-typeprint.obj \
	$(INTDIR)/m2-valprint.obj \
	$(INTDIR)/m2exptab.obj \
	$(INTDIR)/main.obj \
	$(INTDIR)/maint.obj \
	$(INTDIR)/mdebugread.obj \
	$(INTDIR)/mem-break.obj \
	$(INTDIR)/minsyms.obj \
	$(INTDIR)/objfiles.obj \
	$(INTDIR)/parse.obj \
	$(INTDIR)/printcmd.obj \
	$(INTDIR)/remote-utils.obj \
	$(INTDIR)/remote.obj \
	$(INTDIR)/serial.obj \
	$(INTDIR)/source.obj \
	$(INTDIR)/stabsread.obj \
	$(INTDIR)/stabs.obj \
	$(INTDIR)/stack.obj \
	$(INTDIR)/symfile.obj \
	$(INTDIR)/symmisc.obj \
	$(INTDIR)/symtab.obj \
	$(INTDIR)/target.obj \
	$(INTDIR)/thread.obj \
	$(INTDIR)/top.obj \
	$(INTDIR)/typeprint.obj \
	$(INTDIR)/utils.obj \
	$(INTDIR)/valarith.obj \
	$(INTDIR)/valops.obj \
	$(INTDIR)/valprint.obj \
	$(INTDIR)/values.obj \
	$(INTDIR)/monitor.obj \
	$(INTDIR)/nlmread.obj \
	$(INTDIR)/os9kread.obj \
	$(INTDIR)/mipsread.obj \
	$(INTDIR)/callback.obj \
	$(INTDIR)/scm-lang.obj \
	$(INTDIR)/scm-exp.obj \
	$(INTDIR)/scm-valprint.obj \
	$(INTDIR)/gnu-regex.obj \
	$(INTDIR)/dsrec.obj \
	$(INTDIR)/parallel.obj \
	$(INTDIR)/ch-exp.obj \
	$(INTDIR)/bcache.obj \
	$(INTDIR)/debugo.obj

COMMON_SBRS= \
	$(INTDIR)/gui.sbr \
	$(INTDIR)/mainfrm.sbr \
	$(INTDIR)/stdafx.sbr \
	$(INTDIR)/aboutbox.sbr \
	$(INTDIR)/iface.sbr \
	$(INTDIR)/fontinfo.sbr \
	$(INTDIR)/regview.sbr \
	$(INTDIR)/regdoc.sbr \
	$(INTDIR)/bptdoc.sbr \
	$(INTDIR)/colinfo.sbr \
	$(INTDIR)/gdbwrap.sbr \
	$(INTDIR)/srcbrows.sbr \
	$(INTDIR)/scview.sbr \
	$(INTDIR)/framevie.sbr \
	$(INTDIR)/browserl.sbr \
	$(INTDIR)/browserf.sbr \
	$(INTDIR)/gdbdoc.sbr \
	$(INTDIR)/flash.sbr \
	$(INTDIR)/stubs.sbr \
	$(INTDIR)/gdbwin.sbr \
	$(INTDIR)/gdbwinxx.sbr \
	$(INTDIR)/initfake.sbr \
	$(INTDIR)/infoframe.sbr \
	$(INTDIR)/ginfodoc.sbr \
	$(INTDIR)/srcwin.sbr \
	$(INTDIR)/srcd.sbr \
	$(INTDIR)/transbmp.sbr \
	$(INTDIR)/expwin.sbr \
	$(INTDIR)/fsplit.sbr \
	$(INTDIR)/srcsel.sbr \
	$(INTDIR)/props.sbr \
	$(INTDIR)/dirpkr.sbr \
	$(INTDIR)/srcb.sbr \
	$(INTDIR)/mem.sbr \
	$(INTDIR)/bfdcore.sbr \
	$(INTDIR)/change.sbr \
	$(INTDIR)/frameview.sbr \
	$(INTDIR)/log.sbr \
	$(INTDIR)/mini.sbr \
	$(INTDIR)/option.sbr \
	$(INTDIR)/ser-win32s.sbr \
	$(INTDIR)/alloca.sbr \
	$(INTDIR)/argv.sbr \
	$(INTDIR)/bcmp.sbr \
	$(INTDIR)/bzero.sbr \
	$(INTDIR)/obstack.sbr \
	$(INTDIR)/random.sbr \
	$(INTDIR)/rindex.sbr \
	$(INTDIR)/spaces.sbr \
	$(INTDIR)/bcopy.sbr \
	$(INTDIR)/concat.sbr \
	$(INTDIR)/strtod.sbr \
	$(INTDIR)/cplus-dem.sbr \
	$(INTDIR)/vprintf.sbr \
	$(INTDIR)/tmpnam.sbr \
	$(INTDIR)/vasprintf.sbr \
	$(INTDIR)/strdup.sbr \
	$(INTDIR)/getopt1.sbr \
	$(INTDIR)/insque.sbr \
	$(INTDIR)/getopt.sbr \
	$(INTDIR)/hex.sbr \
	$(INTDIR)/getruntime.sbr \
	$(INTDIR)/floatformat.sbr \
	$(INTDIR)/strcasecmp.sbr \
	$(INTDIR)/basename.sbr \
	$(INTDIR)/dis-buf.sbr \
	$(INTDIR)/disassemble.sbr \
	$(INTDIR)/readline.sbr \
	$(INTDIR)/search.sbr \
	$(INTDIR)/signals.sbr \
	$(INTDIR)/keymaps.sbr \
	$(INTDIR)/funmap.sbr \
	$(INTDIR)/isearch.sbr \
	$(INTDIR)/display.sbr \
	$(INTDIR)/parens.sbr \
	$(INTDIR)/bind.sbr \
	$(INTDIR)/rltty.sbr \
	$(INTDIR)/complete.sbr \
	$(INTDIR)/history.sbr \
	$(INTDIR)/archive.sbr \
	$(INTDIR)/archures.sbr \
	$(INTDIR)/bfd.sbr \
	$(INTDIR)/binary.sbr \
	$(INTDIR)/cache.sbr \
	$(INTDIR)/coffgen.sbr \
	$(INTDIR)/cofflink.sbr \
	$(INTDIR)/filemode.sbr \
	$(INTDIR)/format.sbr \
	$(INTDIR)/hash.sbr \
	$(INTDIR)/init.sbr \
	$(INTDIR)/libbfd.sbr \
	$(INTDIR)/linker.sbr \
	$(INTDIR)/opncls.sbr \
	$(INTDIR)/reloc.sbr \
	$(INTDIR)/section.sbr \
	$(INTDIR)/srec.sbr \
	$(INTDIR)/syms.sbr \
	$(INTDIR)/targets.sbr \
	$(INTDIR)/bpt.sbr \
	$(INTDIR)/tekhex.sbr \
	$(INTDIR)/versados.sbr \
	$(INTDIR)/ihex.sbr \
	$(INTDIR)/annotate.sbr \
	$(INTDIR)/blockframe.sbr \
	$(INTDIR)/breakpoint.sbr \
	$(INTDIR)/buildsym.sbr \
	$(INTDIR)/c-lang.sbr \
	$(INTDIR)/c-typeprint.sbr \
	$(INTDIR)/c-valprint.sbr \
	$(INTDIR)/cexptab.sbr \
	$(INTDIR)/ch-lang.sbr \
	$(INTDIR)/ch-typeprint.sbr \
	$(INTDIR)/ch-valprint.sbr \
	$(INTDIR)/coffread.sbr \
	$(INTDIR)/command.sbr \
	$(INTDIR)/complaints.sbr \
	$(INTDIR)/copying.sbr \
	$(INTDIR)/corefile.sbr \
	$(INTDIR)/cp-valprint.sbr \
	$(INTDIR)/dbxread.sbr \
	$(INTDIR)/dcache.sbr \
	$(INTDIR)/demangle.sbr \
	$(INTDIR)/dwarfread.sbr \
	$(INTDIR)/dwarf2read.sbr \
	$(INTDIR)/elfread.sbr \
	$(INTDIR)/environ.sbr \
	$(INTDIR)/eval.sbr \
	$(INTDIR)/exec.sbr \
	$(INTDIR)/expprint.sbr \
	$(INTDIR)/f-lang.sbr \
	$(INTDIR)/f-typeprint.sbr \
	$(INTDIR)/f-valprint.sbr \
	$(INTDIR)/fexptab.sbr \
	$(INTDIR)/findvar.sbr \
	$(INTDIR)/fork-child.sbr \
	$(INTDIR)/gdbtypes.sbr \
	$(INTDIR)/infcmd.sbr \
	$(INTDIR)/inflow.sbr \
	$(INTDIR)/infrun.sbr \
	$(INTDIR)/language.sbr \
	$(INTDIR)/m2-lang.sbr \
	$(INTDIR)/m2-typeprint.sbr \
	$(INTDIR)/m2-valprint.sbr \
	$(INTDIR)/m2exptab.sbr \
	$(INTDIR)/main.sbr \
	$(INTDIR)/maint.sbr \
	$(INTDIR)/mdebugread.sbr \
	$(INTDIR)/mem-break.sbr \
	$(INTDIR)/minsyms.sbr \
	$(INTDIR)/objfiles.sbr \
	$(INTDIR)/parse.sbr \
	$(INTDIR)/printcmd.sbr \
	$(INTDIR)/remote-utils.sbr \
	$(INTDIR)/remote.sbr \
	$(INTDIR)/serial.sbr \
	$(INTDIR)/source.sbr \
	$(INTDIR)/stabsread.sbr \
	$(INTDIR)/stabs.sbr \
	$(INTDIR)/stack.sbr \
	$(INTDIR)/symfile.sbr \
	$(INTDIR)/symmisc.sbr \
	$(INTDIR)/symtab.sbr \
	$(INTDIR)/target.sbr \
	$(INTDIR)/thread.sbr \
	$(INTDIR)/top.sbr \
	$(INTDIR)/typeprint.sbr \
	$(INTDIR)/utils.sbr \
	$(INTDIR)/valarith.sbr \
	$(INTDIR)/valops.sbr \
	$(INTDIR)/valprint.sbr \
	$(INTDIR)/values.sbr \
	$(INTDIR)/monitor.sbr \
	$(INTDIR)/nlmread.sbr \
	$(INTDIR)/os9kread.sbr \
	$(INTDIR)/mipsread.sbr \
	$(INTDIR)/callback.sbr \
	$(INTDIR)/scm-lang.sbr \
	$(INTDIR)/scm-exp.sbr \
	$(INTDIR)/scm-valprint.sbr \
	$(INTDIR)/gnu-regex.sbr \
	$(INTDIR)/dsrec.sbr \
	$(INTDIR)/parallel.sbr \
	$(INTDIR)/ch-exp.sbr \
	$(INTDIR)/bcache.sbr \
	$(INTDIR)/debugo.sbr

COMMON_NMS= \
	$(INTDIR)/gui.nm \
	$(INTDIR)/mainfrm.nm \
	$(INTDIR)/stdafx.nm \
	$(INTDIR)/aboutbox.nm \
	$(INTDIR)/iface.nm \
	$(INTDIR)/fontinfo.nm \
	$(INTDIR)/regview.nm \
	$(INTDIR)/regdoc.nm \
	$(INTDIR)/bptdoc.nm \
	$(INTDIR)/colinfo.nm \
	$(INTDIR)/gdbwrap.nm \
	$(INTDIR)/srcbrows.nm \
	$(INTDIR)/scview.nm \
	$(INTDIR)/framevie.nm \
	$(INTDIR)/browserl.nm \
	$(INTDIR)/browserf.nm \
	$(INTDIR)/gdbdoc.nm \
	$(INTDIR)/flash.nm \
	$(INTDIR)/stubs.nm \
	$(INTDIR)/gdbwin.nm \
	$(INTDIR)/gdbwinxx.nm \
	$(INTDIR)/initfake.nm \
	$(INTDIR)/infoframe.nm \
	$(INTDIR)/ginfodoc.nm \
	$(INTDIR)/srcwin.nm \
	$(INTDIR)/srcd.nm \
	$(INTDIR)/transbmp.nm \
	$(INTDIR)/expwin.nm \
	$(INTDIR)/fsplit.nm \
	$(INTDIR)/srcsel.nm \
	$(INTDIR)/props.nm \
	$(INTDIR)/dirpkr.nm \
	$(INTDIR)/srcb.nm \
	$(INTDIR)/mem.nm \
	$(INTDIR)/bfdcore.nm \
	$(INTDIR)/change.nm \
	$(INTDIR)/frameview.nm \
	$(INTDIR)/log.nm \
	$(INTDIR)/mini.nm \
	$(INTDIR)/option.nm \
	$(INTDIR)/ser-win32s.nm \
	$(INTDIR)/alloca.nm \
	$(INTDIR)/argv.nm \
	$(INTDIR)/bcmp.nm \
	$(INTDIR)/bzero.nm \
	$(INTDIR)/obstack.nm \
	$(INTDIR)/random.nm \
	$(INTDIR)/rindex.nm \
	$(INTDIR)/spaces.nm \
	$(INTDIR)/bcopy.nm \
	$(INTDIR)/concat.nm \
	$(INTDIR)/strtod.nm \
	$(INTDIR)/cplus-dem.nm \
	$(INTDIR)/vprintf.nm \
	$(INTDIR)/tmpnam.nm \
	$(INTDIR)/vasprintf.nm \
	$(INTDIR)/strdup.nm \
	$(INTDIR)/getopt1.nm \
	$(INTDIR)/insque.nm \
	$(INTDIR)/getopt.nm \
	$(INTDIR)/hex.nm \
	$(INTDIR)/getruntime.nm \
	$(INTDIR)/floatformat.nm \
	$(INTDIR)/strcasecmp.nm \
	$(INTDIR)/basename.nm \
	$(INTDIR)/dis-buf.nm \
	$(INTDIR)/disassemble.nm \
	$(INTDIR)/readline.nm \
	$(INTDIR)/search.nm \
	$(INTDIR)/signals.nm \
	$(INTDIR)/keymaps.nm \
	$(INTDIR)/funmap.nm \
	$(INTDIR)/isearch.nm \
	$(INTDIR)/display.nm \
	$(INTDIR)/parens.nm \
	$(INTDIR)/bind.nm \
	$(INTDIR)/rltty.nm \
	$(INTDIR)/complete.nm \
	$(INTDIR)/history.nm \
	$(INTDIR)/archive.nm \
	$(INTDIR)/archures.nm \
	$(INTDIR)/bfd.nm \
	$(INTDIR)/binary.nm \
	$(INTDIR)/cache.nm \
	$(INTDIR)/coffgen.nm \
	$(INTDIR)/cofflink.nm \
	$(INTDIR)/filemode.nm \
	$(INTDIR)/format.nm \
	$(INTDIR)/hash.nm \
	$(INTDIR)/init.nm \
	$(INTDIR)/libbfd.nm \
	$(INTDIR)/linker.nm \
	$(INTDIR)/opncls.nm \
	$(INTDIR)/reloc.nm \
	$(INTDIR)/section.nm \
	$(INTDIR)/srec.nm \
	$(INTDIR)/syms.nm \
	$(INTDIR)/targets.nm \
	$(INTDIR)/bpt.nm \
	$(INTDIR)/tekhex.nm \
	$(INTDIR)/versados.nm \
	$(INTDIR)/ihex.nm \
	$(INTDIR)/annotate.nm \
	$(INTDIR)/blockframe.nm \
	$(INTDIR)/breakpoint.nm \
	$(INTDIR)/buildsym.nm \
	$(INTDIR)/c-lang.nm \
	$(INTDIR)/c-typeprint.nm \
	$(INTDIR)/c-valprint.nm \
	$(INTDIR)/cexptab.nm \
	$(INTDIR)/ch-lang.nm \
	$(INTDIR)/ch-typeprint.nm \
	$(INTDIR)/ch-valprint.nm \
	$(INTDIR)/coffread.nm \
	$(INTDIR)/command.nm \
	$(INTDIR)/complaints.nm \
	$(INTDIR)/copying.nm \
	$(INTDIR)/corefile.nm \
	$(INTDIR)/cp-valprint.nm \
	$(INTDIR)/dbxread.nm \
	$(INTDIR)/dcache.nm \
	$(INTDIR)/demangle.nm \
	$(INTDIR)/dwarfread.nm \
	$(INTDIR)/dwarf2read.nm \
	$(INTDIR)/elfread.nm \
	$(INTDIR)/environ.nm \
	$(INTDIR)/eval.nm \
	$(INTDIR)/exec.nm \
	$(INTDIR)/expprint.nm \
	$(INTDIR)/f-lang.nm \
	$(INTDIR)/f-typeprint.nm \
	$(INTDIR)/f-valprint.nm \
	$(INTDIR)/fexptab.nm \
	$(INTDIR)/findvar.nm \
	$(INTDIR)/fork-child.nm \
	$(INTDIR)/gdbtypes.nm \
	$(INTDIR)/infcmd.nm \
	$(INTDIR)/inflow.nm \
	$(INTDIR)/infrun.nm \
	$(INTDIR)/language.nm \
	$(INTDIR)/m2-lang.nm \
	$(INTDIR)/m2-typeprint.nm \
	$(INTDIR)/m2-valprint.nm \
	$(INTDIR)/m2exptab.nm \
	$(INTDIR)/main.nm \
	$(INTDIR)/maint.nm \
	$(INTDIR)/mdebugread.nm \
	$(INTDIR)/mem-break.nm \
	$(INTDIR)/minsyms.nm \
	$(INTDIR)/objfiles.nm \
	$(INTDIR)/parse.nm \
	$(INTDIR)/printcmd.nm \
	$(INTDIR)/remote-utils.nm \
	$(INTDIR)/remote.nm \
	$(INTDIR)/serial.nm \
	$(INTDIR)/source.nm \
	$(INTDIR)/stabsread.nm \
	$(INTDIR)/stabs.nm \
	$(INTDIR)/stack.nm \
	$(INTDIR)/symfile.nm \
	$(INTDIR)/symmisc.nm \
	$(INTDIR)/symtab.nm \
	$(INTDIR)/target.nm \
	$(INTDIR)/thread.nm \
	$(INTDIR)/top.nm \
	$(INTDIR)/typeprint.nm \
	$(INTDIR)/utils.nm \
	$(INTDIR)/valarith.nm \
	$(INTDIR)/valops.nm \
	$(INTDIR)/valprint.nm \
	$(INTDIR)/values.nm \
	$(INTDIR)/monitor.nm \
	$(INTDIR)/nlmread.nm \
	$(INTDIR)/os9kread.nm \
	$(INTDIR)/mipsread.nm \
	$(INTDIR)/callback.nm \
	$(INTDIR)/scm-lang.nm \
	$(INTDIR)/scm-exp.nm \
	$(INTDIR)/scm-valprint.nm \
	$(INTDIR)/gnu-regex.nm \
	$(INTDIR)/dsrec.nm \
	$(INTDIR)/parallel.nm \
	$(INTDIR)/ch-exp.nm \
	$(INTDIR)/bcache.nm \
	$(INTDIR)/debugo.nm


# How to link gdb

GDB_OBJS=$(COMMON_OBJS) $(TARGET_OBJS)

#@rem $(DECHO) ======== GDB_OBJS=$(GDB_OBJS) ========
$(EXE) : $(OUTDIR) $(DEF_FILE) $(GDB_OBJS) $(EXTRA_DEPS) $(TARGET_DEPS)
    @echo ======== linking $(EXE) =========
    $(DECHO) linkopts: $(LFLAGS) $(TARGET_LFLAGS) 
    $(DECHO) $(LINK32) $(GDB_LFLAGS) $(LFLAGS) 
    $(DECHO) $(EXTRA_DEPS) $(TARGET_LFLAGS) /OUT:$(EXE)
	$(DSET)
    $(LINK32) @<<
  $(GDB_LFLAGS) $(LFLAGS) $(GDB_OBJS) $(EXTRA_DEPS) $(TARGET_LFLAGS) /OUT:$(EXE)
<<

fastlink :
    @echo ======== fastlink $(EXE) =========
    $(LINK32) @<<
  $(GDB_LFLAGS) $(LFLAGS) $(GDB_OBJS) $(EXTRA_DEPS) $(TARGET_LFLAGS) /OUT:$(EXE)
<<

$(OUTDIR)\..\gdb-$(TARGET).exe : $(OUTDIR)  $(DEF_FILE) $(GDB_OBJS)
    @echo ======== linking $(OUTDIR)\..\gdb-$(TARGET).exe =========
    $(LINK32) @<<
  $(LINK32_FLAGS) $(GDB_OBJS) /OUT:$(OUTDIR)\..\gdb-$(TARGET).exe
<<

# How to build serdll32.{dll,lib}

#!include "serdll.mak"
$(OUTDIR)\serdll32.dll $(OUTDIR)\serdll16.dll : serdll.mak
	nmake -f serdll.mak

# How to build resource file.

GDB_RESS=\
	$(MSWINDIR)\gui.rc\
	$(MSWINDIR)\res\guidoc.ico\
	$(MSWINDIR)\res\idr_cmdf.ico\
	$(MSWINDIR)\res\ico00001.ico\
	$(MSWINDIR)\res\idr_srct.ico\
	$(MSWINDIR)\res\idr_srcb.ico\
	$(MSWINDIR)\res\ico00004.ico\
	$(MSWINDIR)\res\idr_regt.ico\
	$(MSWINDIR)\res\idr_main.ico\
	$(MSWINDIR)\res\infodoc.ico\
	$(MSWINDIR)\res\N_gdb.ico\
	$(MSWINDIR)\res\icon1.ico\
	$(MSWINDIR)\res\memdoc.ico\
	$(MSWINDIR)\res\id_sym_i.ico\
	$(MSWINDIR)\res\icon2.ico\
	$(MSWINDIR)\res\toolbar.bmp\
	$(MSWINDIR)\res\bitmap1.bmp\
	$(MSWINDIR)\res\idr_main.bmp\
	$(MSWINDIR)\res\idr_wint.bmp\
	$(MSWINDIR)\res\bmp00007.bmp\
	$(MSWINDIR)\res\bmp00001.bmp\
	$(MSWINDIR)\res\bmp00002.bmp\
	$(MSWINDIR)\res\bmp00003.bmp\
	$(MSWINDIR)\res\cdown.bmp\
	$(MSWINDIR)\res\cf.bmp\
	$(MSWINDIR)\res\bmp00004.bmp\
	$(MSWINDIR)\res\cursor1.cur\
	$(MSWINDIR)\res\cur00001.cur\
	$(MSWINDIR)\res\bpt_curs.cur\
	$(MSWINDIR)\res\gui.rc2\
	$(MSWINDIR)\menus\menus.rc


SOURCE=$(MSWINDIR)\gui.rc
RSC_PROJ=/l 0x409 /fo$(INTDIR)/"gui.res" /d "$(DBGDEF)" 
DEP_GUI_R=\
	$(MSWINDIR)\res\guidoc.ico\
	$(MSWINDIR)\res\idr_cmdf.ico\
	$(MSWINDIR)\res\ico00001.ico\
	$(MSWINDIR)\res\idr_srct.ico\
	$(MSWINDIR)\res\idr_srcb.ico\
	$(MSWINDIR)\res\ico00004.ico\
	$(MSWINDIR)\res\idr_regt.ico\
	$(MSWINDIR)\res\idr_main.ico\
	$(MSWINDIR)\res\infodoc.ico\
	$(MSWINDIR)\res\N_gdb.ico\
	$(MSWINDIR)\res\icon1.ico\
	$(MSWINDIR)\res\memdoc.ico\
	$(MSWINDIR)\res\id_sym_i.ico\
	$(MSWINDIR)\res\icon2.ico\
	$(MSWINDIR)\res\toolbar.bmp\
	$(MSWINDIR)\res\bitmap1.bmp\
	$(MSWINDIR)\res\idr_main.bmp\
	$(MSWINDIR)\res\idr_wint.bmp\
	$(MSWINDIR)\res\bmp00007.bmp\
	$(MSWINDIR)\res\bmp00001.bmp\
	$(MSWINDIR)\res\bmp00002.bmp\
	$(MSWINDIR)\res\bmp00003.bmp\
	$(MSWINDIR)\res\cdown.bmp\
	$(MSWINDIR)\res\cf.bmp\
	$(MSWINDIR)\res\bmp00004.bmp\
	$(MSWINDIR)\res\cursor1.cur\
	$(MSWINDIR)\res\cur00001.cur\
	$(MSWINDIR)\res\bpt_curs.cur\
	$(MSWINDIR)\res\gui.rc2\
	$(MSWINDIR)\menus\menus.rc

$(INTDIR)/gui.res :  $(SOURCE)  $(DEP_GUI_R) $(INTDIR)
   $(RSC) $(RSC_PROJ)  $(SOURCE) 

# ALL OPTIONS MUST COME FIRST!!!
#$(INTDIR)/$(GDB).res :  $(GDB_RESS) $(INTDIR)
#    @echo ======== building $(INTDIR)\$(GDB).res =========
#   @rem $(RSC) $(RSC_FLAGS) /fo$(INTDIR)/$(GDB).res $(GDB_RESS) 
#   $(RSC) /l 0x409 /fo$(INTDIR)/"$(GDB).res" /d "_DEBUG"  $(GDB_RESS) 


# How to build other less useful files.

{$(MSWINDIR)}.cpp{$(INTDIR)}.sbr: 
   $(DECHO) $(CPP) $(GDB_CFLAGS) $(CFLAGS) /FR$(INTDIR)/ $<
   $(CPP) @<<
$(GDB_CFLAGS) $(CFLAGS) /FR$(INTDIR)/ $<
<<  

{$(MSWINDIR)}.c{$(INTDIR)}.sbr: 
   $(DECHO) $(CPP) $(GDB_CFLAGS) $(CFLAGS) /FR$(INTDIR)/ $<
   $(CPP) @<<
$(GDB_CFLAGS) $(CFLAGS) /FR$(INTDIR)/ $<
<<  

{$(SRCDIR)}.c{$(INTDIR)}.sbr: 
   $(DECHO) $(CPP) $(GDB_CFLAGS) $(CFLAGS) /FR$(INTDIR)/ $<
   $(CPP) @<<
$(GDB_CFLAGS) $(CFLAGS) /FR$(INTDIR)/ $<
<<  

{$(SRCDIR)/bfd}.c{$(INTDIR)}.sbr: 
   $(DECHO) $(CPP) $(GDB_CFLAGS) $(CFLAGS) /FR$(INTDIR)/ $<
   $(CPP) @<<
   $(GDB_CFLAGS) $(CFLAGS) /FR$(INTDIR)/ $<
<<  

{$(SRCDIR)/libiberty}.c{$(INTDIR)}.sbr: 
   $(DECHO) $(CPP) $(GDB_CFLAGS) $(CFLAGS) /FR$(INTDIR)/ $<
   $(CPP) @<<
$(GDB_CFLAGS) $(CFLAGS) /FR$(INTDIR)/ $<
<<  

{$(SRCDIR)/readline}.c{$(INTDIR)}.sbr: 
   $(DECHO) $(CPP) $(GDB_CFLAGS) $(CFLAGS) /FR$(INTDIR)/ $<
   $(CPP) @<<
$(GDB_CFLAGS) $(CFLAGS) /FR$(INTDIR)/ $<
<<  

{$(SRCDIR)/opcodes}.c{$(INTDIR)}.sbr: 
   $(DECHO) $(CPP) $(GDB_CFLAGS) $(CFLAGS) /FR$(INTDIR)/ $<
   $(CPP) @<<
$(GDB_CFLAGS) $(CFLAGS) /FR$(INTDIR)/ $<
<<  

{$(MSWINDIR)/gdb}.c{$(INTDIR)}.sbr: 
   $(DECHO) $(CPP) $(GDB_CFLAGS) $(CFLAGS) /FR$(INTDIR)/ $<
   $(CPP) @<<
$(GDB_CFLAGS) $(CFLAGS) /FR$(INTDIR)/ $<
<<  

{$(SRCDIR)/gdb/mswin/prebuilt/gdb}.c{$(INTDIR)}.sbr: 
   $(DECHO) $(CPP) $(GDB_CFLAGS) $(CFLAGS) /FR$(INTDIR)/ $<
   $(CPP) @<<
$(GDB_CFLAGS) $(CFLAGS) /FR$(INTDIR)/ $<
<<  

#targets:
# a29k h8300 i386 m68k mips sh sparclite sparclet

{$(SRCDIR)/gdb/mswin/prebuilt/$(TARGET)}.c{$(INTDIR)}.sbr: 
   $(DECHO) $(CPP) $(GDB_CFLAGS) $(CFLAGS) /FR$(INTDIR)/ $<
   $(CPP) @<<
$(GDB_CFLAGS) $(CFLAGS) /FR$(INTDIR)/ $<
<<  

{$(SRCDIR)/sim/$(TARGET)}.c{$(INTDIR)}.sbr: 
   $(DECHO) $(CPP) $(GDB_CFLAGS) $(CFLAGS) /FR$(INTDIR)/ $<
   $(CPP) @<<
$(GDB_CFLAGS) $(CFLAGS) /FR$(INTDIR)/ $<
<<  


GDB_NMS=$(COMMON_NMS) $(TARGET_NMS)

#@rem w:\nm --defined-only --extern-only --print-file-name %%f 
#@rem $(OUTDIR)\$(GDB).sym
    #set SOURCE=$<
    #concopy $@ <<
    #echo h:\bin\err d:\mksnt\nm -A %SOURCE% | grep -v " ? " | grep -v " U " 
    #echo $@ 
#exit
#<<

{$(INTDIR)}.obj{$(INTDIR)}.nm: 
	$(DECHO) $(UNIXDIR)\nm --defined-only --extern-only --print-file-name --out $(INTDIR)\$@ $< 
	@rem $(UNIXDIR)\nm --defined-only --extern-only --print-file-name --out $(INTDIR)\$@ $< 
	@rem err $(UNIXDIR)\nm -A $< | grep -v " ? " | grep -v " U " 


$(NMS): $(GDB_NMS)
    @echo ======== creating $(NMS) =========
	del $(NMS)
	@rem for %%f in ($(GDB_NMS)) do type %%f >> $(NMS)
	for %%f in ($(INTDIR)\*.nm) do type %%f >> $(NMS)


GDB_SBRS=$(COMMON_SBRS) $(TARGET_SBRS)

#$(DECHO) $(GDB_SBRS)
#note the /o must come first!!
$(BSC): $(OUTDIR)  $(GDB_SBRS)
    @echo ======== creating $(BSC) =========
    $(DECHO) $(BSC32) -o $(BSC) $(BSC32_FLAGS) 
    $(BSC32) @<<
  $(BSC32_FLAGS) /o $(BSC) $(GDB_SBRS) 
<<



# Dependencies for common objects.

$(INTDIR)/archive.obj : $(SRCDIR)/bfd/archive.c
$(INTDIR)/archures.obj : $(SRCDIR)/bfd/archures.c
$(INTDIR)/bfd.obj : $(SRCDIR)/bfd/bfd.c
$(INTDIR)/binary.obj : $(SRCDIR)/bfd/binary.c
$(INTDIR)/cache.obj : $(SRCDIR)/bfd/cache.c
$(INTDIR)/coffgen.obj : $(SRCDIR)/bfd/coffgen.c
$(INTDIR)/cofflink.obj : $(SRCDIR)/bfd/cofflink.c
$(INTDIR)/filemode.obj : $(SRCDIR)/bfd/filemode.c
$(INTDIR)/format.obj : $(SRCDIR)/bfd/format.c
$(INTDIR)/hash.obj : $(SRCDIR)/bfd/hash.c
$(INTDIR)/ihex.obj : $(SRCDIR)/bfd/ihex.c
$(INTDIR)/init.obj : $(SRCDIR)/bfd/init.c
$(INTDIR)/libbfd.obj : $(SRCDIR)/bfd/libbfd.c
$(INTDIR)/linker.obj : $(SRCDIR)/bfd/linker.c
$(INTDIR)/opncls.obj : $(SRCDIR)/bfd/opncls.c
$(INTDIR)/reloc.obj : $(SRCDIR)/bfd/reloc.c
$(INTDIR)/section.obj : $(SRCDIR)/bfd/section.c
$(INTDIR)/srec.obj : $(SRCDIR)/bfd/srec.c
$(INTDIR)/stabs.obj : $(SRCDIR)/bfd/stabs.c
$(INTDIR)/syms.obj : $(SRCDIR)/bfd/syms.c
$(INTDIR)/targets.obj : $(SRCDIR)/bfd/targets.c
$(INTDIR)/tekhex.obj : $(SRCDIR)/bfd/tekhex.c
$(INTDIR)/versados.obj : $(SRCDIR)/bfd/versados.c

$(INTDIR)/a29k-tdep.obj : $(SRCDIR)/gdb/a29k-tdep.c
$(INTDIR)/bcache.obj : $(SRCDIR)/gdb/bcache.c
$(INTDIR)/c-lang.obj : $(SRCDIR)/gdb/c-lang.c
$(INTDIR)/c-typeprint.obj : $(SRCDIR)/gdb/c-typeprint.c
$(INTDIR)/c-valprint.obj : $(SRCDIR)/gdb/c-valprint.c
$(INTDIR)/callback.obj : $(SRCDIR)/gdb/callback.c
$(INTDIR)/ch-exp.obj : $(SRCDIR)/gdb/ch-exp.c
$(INTDIR)/ch-lang.obj : $(SRCDIR)/gdb/ch-lang.c
$(INTDIR)/ch-typeprint.obj : $(SRCDIR)/gdb/ch-typeprint.c
$(INTDIR)/ch-valprint.obj : $(SRCDIR)/gdb/ch-valprint.c
$(INTDIR)/cp-valprint.obj : $(SRCDIR)/gdb/cp-valprint.c
$(INTDIR)/dbxread.obj : $(SRCDIR)/gdb/dbxread.c
$(INTDIR)/dsrec.obj : $(SRCDIR)/gdb/dsrec.c
$(INTDIR)/dwarfread.obj : $(SRCDIR)/gdb/dwarfread.c
$(INTDIR)/dwarf2read.obj : $(SRCDIR)/gdb/dwarf2read.c
$(INTDIR)/elfread.obj : $(SRCDIR)/gdb/elfread.c
$(INTDIR)/f-exp.tab.obj : $(SRCDIR)/gdb/f-exp.tab.c
$(INTDIR)/f-lang.obj : $(SRCDIR)/gdb/f-lang.c
$(INTDIR)/f-typeprint.obj : $(SRCDIR)/gdb/f-typeprint.c
$(INTDIR)/f-valprint.obj : $(SRCDIR)/gdb/f-valprint.c
$(INTDIR)/gnu-regex.obj : $(SRCDIR)/gdb/gnu-regex.c
$(INTDIR)/m2-lang.obj : $(SRCDIR)/gdb/m2-lang.c
$(INTDIR)/m2-typeprint.obj : $(SRCDIR)/gdb/m2-typeprint.c
$(INTDIR)/m2-valprint.obj : $(SRCDIR)/gdb/m2-valprint.c
$(INTDIR)/mdebugread.obj : $(SRCDIR)/gdb/mdebugread.c
$(INTDIR)/mem-break.obj : $(SRCDIR)/gdb/mem-break.c
$(INTDIR)/mipsread.obj : $(SRCDIR)/gdb/mipsread.c
$(INTDIR)/monitor.obj : $(SRCDIR)/gdb/monitor.c
$(INTDIR)/nlmread.obj : $(SRCDIR)/gdb/nlmread.c
$(INTDIR)/os9kread.obj : $(SRCDIR)/gdb/os9kread.c
$(INTDIR)/remote-utils.obj : $(SRCDIR)/gdb/remote-utils.c
$(INTDIR)/scm-exp.obj : $(SRCDIR)/gdb/scm-exp.c
$(INTDIR)/scm-lang.obj : $(SRCDIR)/gdb/scm-lang.c
$(INTDIR)/scm-valprint.obj : $(SRCDIR)/gdb/scm-valprint.c

$(INTDIR)/alloca.obj : $(SRCDIR)/libiberty/alloca.c
$(INTDIR)/argv.obj : $(SRCDIR)/libiberty/argv.c
$(INTDIR)/basename.obj : $(SRCDIR)/libiberty/basename.c
$(INTDIR)/bcmp.obj : $(SRCDIR)/libiberty/bcmp.c
$(INTDIR)/bcopy.obj : $(SRCDIR)/libiberty/bcopy.c
$(INTDIR)/bzero.obj : $(SRCDIR)/libiberty/bzero.c
$(INTDIR)/concat.obj : $(SRCDIR)/libiberty/concat.c
$(INTDIR)/cplus-dem.obj : $(SRCDIR)/libiberty/cplus-dem.c
$(INTDIR)/floatformat.obj : $(SRCDIR)/libiberty/floatformat.c
$(INTDIR)/getopt.obj : $(SRCDIR)/libiberty/getopt.c
$(INTDIR)/getopt1.obj : $(SRCDIR)/libiberty/getopt1.c
$(INTDIR)/getruntime.obj : $(SRCDIR)/libiberty/getruntime.c
$(INTDIR)/hex.obj : $(SRCDIR)/libiberty/hex.c
$(INTDIR)/insque.obj : $(SRCDIR)/libiberty/insque.c
$(INTDIR)/obstack.obj : $(SRCDIR)/libiberty/obstack.c
$(INTDIR)/random.obj : $(SRCDIR)/libiberty/random.c
$(INTDIR)/rindex.obj : $(SRCDIR)/libiberty/rindex.c
$(INTDIR)/spaces.obj : $(SRCDIR)/libiberty/spaces.c
$(INTDIR)/strcasecmp.obj : $(SRCDIR)/libiberty/strcasecmp.c
$(INTDIR)/strdup.obj : $(SRCDIR)/libiberty/strdup.c
$(INTDIR)/strtod.obj : $(SRCDIR)/libiberty/strtod.c
$(INTDIR)/tmpnam.obj : $(SRCDIR)/libiberty/tmpnam.c
$(INTDIR)/vasprintf.obj : $(SRCDIR)/libiberty/vasprintf.c
$(INTDIR)/debugo.obj : $(SRCDIR)/libiberty/debugo.c
$(INTDIR)/vprintf.obj : $(SRCDIR)/libiberty/vprintf.c

$(INTDIR)/dis-buf.obj : $(SRCDIR)/opcodes/dis-buf.c
$(INTDIR)/disassemble.obj : $(SRCDIR)/opcodes/disassemble.c

$(INTDIR)/bind.obj : $(SRCDIR)/readline/bind.c
$(INTDIR)/complete.obj : $(SRCDIR)/readline/complete.c
$(INTDIR)/display.obj : $(SRCDIR)/readline/display.c
$(INTDIR)/funmap.obj : $(SRCDIR)/readline/funmap.c
$(INTDIR)/history.obj : $(SRCDIR)/readline/history.c
$(INTDIR)/isearch.obj : $(SRCDIR)/readline/isearch.c
$(INTDIR)/keymaps.obj : $(SRCDIR)/readline/keymaps.c
$(INTDIR)/parens.obj : $(SRCDIR)/readline/parens.c
$(INTDIR)/readline.obj : $(SRCDIR)/readline/readline.c
$(INTDIR)/rltty.obj : $(SRCDIR)/readline/rltty.c
$(INTDIR)/search.obj : $(SRCDIR)/readline/search.c
$(INTDIR)/signals.obj : $(SRCDIR)/readline/signals.c

$(INTDIR)/annotate.obj : $(SRCDIR)/gdb/annotate.c
$(INTDIR)/blockframe.obj : $(SRCDIR)/gdb/blockframe.c
$(INTDIR)/breakpoint.obj : $(SRCDIR)/gdb/breakpoint.c
$(INTDIR)/buildsym.obj : $(SRCDIR)/gdb/buildsym.c
$(INTDIR)/coffread.obj : $(SRCDIR)/gdb/coffread.c
$(INTDIR)/command.obj : $(SRCDIR)/gdb/command.c
$(INTDIR)/complaints.obj : $(SRCDIR)/gdb/complaints.c
$(INTDIR)/corefile.obj : $(SRCDIR)/gdb/corefile.c
$(INTDIR)/dcache.obj : $(SRCDIR)/gdb/dcache.c
$(INTDIR)/demangle.obj : $(SRCDIR)/gdb/demangle.c
$(INTDIR)/environ.obj : $(SRCDIR)/gdb/environ.c
$(INTDIR)/eval.obj : $(SRCDIR)/gdb/eval.c
$(INTDIR)/exec.obj : $(SRCDIR)/gdb/exec.c
$(INTDIR)/expprint.obj : $(SRCDIR)/gdb/expprint.c
$(INTDIR)/findvar.obj : $(SRCDIR)/gdb/findvar.c
$(INTDIR)/gdbtypes.obj : $(SRCDIR)/gdb/gdbtypes.c
$(INTDIR)/infcmd.obj : $(SRCDIR)/gdb/infcmd.c
$(INTDIR)/inflow.obj : $(SRCDIR)/gdb/inflow.c
$(INTDIR)/infrun.obj : $(SRCDIR)/gdb/infrun.c
$(INTDIR)/language.obj : $(SRCDIR)/gdb/language.c
$(INTDIR)/main.obj : $(SRCDIR)/gdb/main.c
$(INTDIR)/maint.obj : $(SRCDIR)/gdb/maint.c
$(INTDIR)/minsyms.obj : $(SRCDIR)/gdb/minsyms.c
$(INTDIR)/objfiles.obj : $(SRCDIR)/gdb/objfiles.c
$(INTDIR)/parse.obj : $(SRCDIR)/gdb/parse.c
$(INTDIR)/printcmd.obj : $(SRCDIR)/gdb/printcmd.c
$(INTDIR)/remote.obj : $(SRCDIR)/gdb/remote.c
$(INTDIR)/serial.obj : $(SRCDIR)/gdb/serial.c
$(INTDIR)/source.obj : $(SRCDIR)/gdb/source.c
$(INTDIR)/stabsread.obj : $(SRCDIR)/gdb/stabsread.c
$(INTDIR)/stack.obj : $(SRCDIR)/gdb/stack.c
$(INTDIR)/symfile.obj : $(SRCDIR)/gdb/symfile.c
$(INTDIR)/symmisc.obj : $(SRCDIR)/gdb/symmisc.c
$(INTDIR)/symtab.obj : $(SRCDIR)/gdb/symtab.c
$(INTDIR)/target.obj : $(SRCDIR)/gdb/target.c
$(INTDIR)/thread.obj : $(SRCDIR)/gdb/thread.c
$(INTDIR)/top.obj : $(SRCDIR)/gdb/top.c
$(INTDIR)/typeprint.obj : $(SRCDIR)/gdb/typeprint.c
$(INTDIR)/utils.obj : $(SRCDIR)/gdb/utils.c
$(INTDIR)/valarith.obj : $(SRCDIR)/gdb/valarith.c
$(INTDIR)/valops.obj : $(SRCDIR)/gdb/valops.c
$(INTDIR)/valprint.obj : $(SRCDIR)/gdb/valprint.c
$(INTDIR)/values.obj : $(SRCDIR)/gdb/values.c

$(INTDIR)/aboutbox.obj : $(SRCDIR)/gdb/mswin/aboutbox.cpp
$(INTDIR)/bfdcore.obj : $(SRCDIR)/gdb/mswin/bfdcore.c
$(INTDIR)/bpt.obj : $(SRCDIR)/gdb/mswin/bpt.cpp
$(INTDIR)/bptdoc.obj : $(SRCDIR)/gdb/mswin/bptdoc.cpp
$(INTDIR)/browserf.obj : $(SRCDIR)/gdb/mswin/browserf.cpp
$(INTDIR)/browserl.obj : $(SRCDIR)/gdb/mswin/browserl.cpp
$(INTDIR)/change.obj : $(SRCDIR)/gdb/mswin/change.cpp
$(INTDIR)/colinfo.obj : $(SRCDIR)/gdb/mswin/colinfo.cpp
$(INTDIR)/dirpkr.obj : $(SRCDIR)/gdb/mswin/dirpkr.cpp
$(INTDIR)/expwin.obj : $(SRCDIR)/gdb/mswin/expwin.cpp
$(INTDIR)/flash.obj : $(SRCDIR)/gdb/mswin/flash.cpp
$(INTDIR)/fontinfo.obj : $(SRCDIR)/gdb/mswin/fontinfo.cpp
$(INTDIR)/framevie.obj : $(SRCDIR)/gdb/mswin/framevie.cpp
$(INTDIR)/frameview.obj : $(SRCDIR)/gdb/mswin/frameview.cpp
$(INTDIR)/fsplit.obj : $(SRCDIR)/gdb/mswin/fsplit.cpp
$(INTDIR)/gdbdoc.obj : $(SRCDIR)/gdb/mswin/gdbdoc.cpp
$(INTDIR)/gdbwin.obj : $(SRCDIR)/gdb/mswin/gdbwin.c
$(INTDIR)/gdbwinxx.obj : $(SRCDIR)/gdb/mswin/gdbwinxx.cpp
$(INTDIR)/gdbwrap.obj : $(SRCDIR)/gdb/mswin/gdbwrap.cpp
$(INTDIR)/ginfodoc.obj : $(SRCDIR)/gdb/mswin/ginfodoc.cpp
$(INTDIR)/gui.obj : $(SRCDIR)/gdb/mswin/gui.cpp
$(INTDIR)/iface.obj : $(SRCDIR)/gdb/mswin/iface.cpp
$(INTDIR)/infoframe.obj : $(SRCDIR)/gdb/mswin/infoframe.cpp
$(INTDIR)/initfake.obj : $(SRCDIR)/gdb/mswin/initfake.c
$(INTDIR)/log.obj : $(SRCDIR)/gdb/mswin/log.cpp
$(INTDIR)/mainfrm.obj : $(SRCDIR)/gdb/mswin/mainfrm.cpp
$(INTDIR)/mem.obj : $(SRCDIR)/gdb/mswin/mem.cpp
$(INTDIR)/mini.obj : $(SRCDIR)/gdb/mswin/mini.cpp
$(INTDIR)/option.obj : $(SRCDIR)/gdb/mswin/option.cpp
$(INTDIR)/parallel.obj : $(SRCDIR)/gdb/mswin/parallel.cpp
$(INTDIR)/props.obj : $(SRCDIR)/gdb/mswin/props.cpp
$(INTDIR)/regdoc.obj : $(SRCDIR)/gdb/mswin/regdoc.cpp
$(INTDIR)/regview.obj : $(SRCDIR)/gdb/mswin/regview.cpp
$(INTDIR)/scview.obj : $(SRCDIR)/gdb/mswin/scview.cpp
$(INTDIR)/ser-win32s.obj : $(SRCDIR)/gdb/mswin/ser-win32s.c
$(INTDIR)/srcb.obj : $(SRCDIR)/gdb/mswin/srcb.cpp
$(INTDIR)/srcbrows.obj : $(SRCDIR)/gdb/mswin/srcbrows.cpp
$(INTDIR)/srcd.obj : $(SRCDIR)/gdb/mswin/srcd.cpp
$(INTDIR)/srcsel.obj : $(SRCDIR)/gdb/mswin/srcsel.cpp
$(INTDIR)/srcwin.obj : $(SRCDIR)/gdb/mswin/srcwin.cpp
$(INTDIR)/stdafx.obj : $(SRCDIR)/gdb/mswin/stdafx.cpp
$(INTDIR)/stubs.obj : $(SRCDIR)/gdb/mswin/stubs.c
$(INTDIR)/transbmp.obj : $(SRCDIR)/gdb/mswin/transbmp.cpp

$(INTDIR)/cexptab.obj : $(SRCDIR)/gdb/mswin/prebuilt/gdb/cexptab.c
$(INTDIR)/copying.obj : $(SRCDIR)/gdb/mswin/prebuilt/gdb/copying.c
$(INTDIR)/fexptab.obj : $(SRCDIR)/gdb/mswin/prebuilt/gdb/fexptab.c
$(INTDIR)/m2exptab.obj : $(SRCDIR)/gdb/mswin/prebuilt/gdb/m2exptab.c

# Dependencies for target-dependent objects that don't conflict
# with each other.

$(INTDIR)/aout0.obj : $(SRCDIR)/bfd/aout0.c
$(INTDIR)/aout32.obj : $(SRCDIR)/bfd/aout32.c
$(INTDIR)/coff-a29k.obj : $(SRCDIR)/bfd/coff-a29k.c
$(INTDIR)/coff-h8300.obj : $(SRCDIR)/bfd/coff-h8300.c
$(INTDIR)/coff-m68k.obj : $(SRCDIR)/bfd/coff-m68k.c
$(INTDIR)/coff-mips.obj : $(SRCDIR)/bfd/coff-mips.c
$(INTDIR)/coff-sh.obj : $(SRCDIR)/bfd/coff-sh.c
$(INTDIR)/coff-sparc.obj : $(SRCDIR)/bfd/coff-sparc.c
$(INTDIR)/cpu-a29k.obj : $(SRCDIR)/bfd/cpu-a29k.c
$(INTDIR)/cpu-h8300.obj : $(SRCDIR)/bfd/cpu-h8300.c
$(INTDIR)/cpu-i386.obj : $(SRCDIR)/bfd/cpu-i386.c
$(INTDIR)/cpu-m68k.obj : $(SRCDIR)/bfd/cpu-m68k.c
$(INTDIR)/cpu-mips.obj : $(SRCDIR)/bfd/cpu-mips.c
$(INTDIR)/cpu-sh.obj : $(SRCDIR)/bfd/cpu-sh.c
$(INTDIR)/cpu-sparc.obj : $(SRCDIR)/bfd/cpu-sparc.c
$(INTDIR)/ecoff.obj : $(SRCDIR)/bfd/ecoff.c
$(INTDIR)/ecofflink.obj : $(SRCDIR)/bfd/ecofflink.c
$(INTDIR)/elf.obj : $(SRCDIR)/bfd/elf.c
$(INTDIR)/elf32-mips.obj : $(SRCDIR)/bfd/elf32-mips.c
$(INTDIR)/elf32.obj : $(SRCDIR)/bfd/elf32.c
$(INTDIR)/elflink.obj : $(SRCDIR)/bfd/elflink.c
$(INTDIR)/i386aout.obj : $(SRCDIR)/bfd/i386aout.c
$(INTDIR)/reloc16.obj : $(SRCDIR)/bfd/reloc16.c
$(INTDIR)/stab-syms.obj : $(SRCDIR)/bfd/stab-syms.c
$(INTDIR)/sunos.obj : $(SRCDIR)/bfd/sunos.c

$(INTDIR)/cpu32bug-rom.obj : $(SRCDIR)/gdb/cpu32bug-rom.c
$(INTDIR)/h8300-tdep.obj : $(SRCDIR)/gdb/h8300-tdep.c
$(INTDIR)/i386-tdep.obj : $(SRCDIR)/gdb/i386-tdep.c
$(INTDIR)/m68k-tdep.obj : $(SRCDIR)/gdb/m68k-tdep.c
$(INTDIR)/mips-tdep.obj : $(SRCDIR)/gdb/mips-tdep.c
$(INTDIR)/remote-e7000.obj : $(SRCDIR)/gdb/remote-e7000.c
$(INTDIR)/remote-est.obj : $(SRCDIR)/gdb/remote-est.c
$(INTDIR)/remote-hms.obj : $(SRCDIR)/gdb/remote-hms.c
$(INTDIR)/remote-mips.obj : $(SRCDIR)/gdb/remote-mips.c
$(INTDIR)/remote-sim.obj : $(SRCDIR)/gdb/remote-sim.c
$(INTDIR)/remote-udi.obj : $(SRCDIR)/gdb/remote-udi.c
$(INTDIR)/rom68k-rom.obj : $(SRCDIR)/gdb/rom68k-rom.c
$(INTDIR)/ser-e7kpc.obj : $(SRCDIR)/gdb/ser-e7kpc.c
$(INTDIR)/sh-tdep.obj : $(SRCDIR)/gdb/sh-tdep.c
$(INTDIR)/sh3-rom.obj : $(SRCDIR)/gdb/sh3-rom.c
$(INTDIR)/sparc-tdep.obj : $(SRCDIR)/gdb/sparc-tdep.c
$(INTDIR)/sparcl-tdep.obj : $(SRCDIR)/gdb/sparcl-tdep.c
$(INTDIR)/sparclet-rom.obj : $(SRCDIR)/gdb/sparclet-rom.c

$(INTDIR)/udi2go32.obj : $(SRCDIR)/gdb/29k-share/udi/udi2go32.c
$(INTDIR)/udip2soc.obj : $(SRCDIR)/gdb/29k-share/udi/udip2soc.c
$(INTDIR)/udr.obj : $(SRCDIR)/gdb/29k-share/udi/udr.c

$(INTDIR)/a29k-dis.obj : $(SRCDIR)/opcodes/a29k-dis.c
$(INTDIR)/h8300-dis.obj : $(SRCDIR)/opcodes/h8300-dis.c
$(INTDIR)/i386-dis.obj : $(SRCDIR)/opcodes/i386-dis.c
$(INTDIR)/m68k-dis.obj : $(SRCDIR)/opcodes/m68k-dis.c
$(INTDIR)/m68k-opc.obj : $(SRCDIR)/opcodes/m68k-opc.c
$(INTDIR)/mips-dis.obj : $(SRCDIR)/opcodes/mips-dis.c
$(INTDIR)/mips-opc.obj : $(SRCDIR)/opcodes/mips-opc.c
$(INTDIR)/sh-dis.obj : $(SRCDIR)/opcodes/sh-dis.c
$(INTDIR)/sparc-dis.obj : $(SRCDIR)/opcodes/sparc-dis.c
$(INTDIR)/sparc-opc.obj : $(SRCDIR)/opcodes/sparc-opc.c


# Dependences for target-dependent objects.  
# These get included into target-specific makefiles.
$(INTDIR)/version.obj : $(SRCDIR)/gdb/mswin/prebuilt/$(TARGET)/version.c


cleanout:
	-del $(OUTDIR)\*.map > nul
	-del $(OUTDIR)\*.exe > nul
	-del $(OUTDIR)\*.dll > nul
	-del $(OUTDIR)\*.bsc > nul
	-del $(OUTDIR)\*.lib > nul
	-del $(OUTDIR)\*.exp > nul
	-del $(OUTDIR)\*.nms > nul
	-del $(OUTDIR)\*.pdb > nul
	-del $(OUTDIR)\*.pch > nul
	-del $(OUTDIR)\*.ilk > nul

clean: cleanout
	-del $(INTDIR)\*.obj > nul
	-del $(INTDIR)\*.sbr > nul
	-del $(INTDIR)\*.lib > nul
	-del $(INTDIR)\*.exp > nul
	-del $(INTDIR)\*.nm > nul
	-del $(INTDIR)\sim\*.obj > nul
	-del $(INTDIR)\sim\*.sbr > nul
	-del $(INTDIR)\sim\*.nm > nul
