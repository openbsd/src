# Microsoft Visual C++ Generated NMAKE File, Format Version 2.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

!IF "$(CFG)" == ""
CFG=sh
!MESSAGE No configuration specified.  Defaulting to sh.
!ENDIF 

!IF "$(CFG)" != "sh" && "$(CFG)" != "Win32 Release" && "$(CFG)" != "h8300" &&\
 "$(CFG)" != "m68k" && "$(CFG)" != "SparcLite" && "$(CFG)" != "mips" && "$(CFG)"\
 != "a29k" && "$(CFG)" != "i386"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE on this makefile
!MESSAGE by defining the macro CFG on the command line.  For example:
!MESSAGE 
!MESSAGE NMAKE /f "gui.mak" CFG="sh"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "sh" (based on "Win32 (x86) Application")
!MESSAGE "Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "h8300" (based on "Win32 (x86) Application")
!MESSAGE "m68k" (based on "Win32 (x86) Application")
!MESSAGE "SparcLite" (based on "Win32 (x86) Application")
!MESSAGE "mips" (based on "Win32 (x86) Application")
!MESSAGE "a29k" (based on "Win32 (x86) Application")
!MESSAGE "i386" (based on "Win32 (x86) Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

################################################################################
# Begin Project
# PROP Target_Last_Scanned "sh"
MTL=MkTypLib.exe
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "sh"

# PROP BASE Use_MFC 6
# PROP BASE Use_Debug_Libraries 1
# PROP Use_MFC 5
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "c:\gs\sh"
# PROP Intermediate_Dir "c:\gs\tmp\sh"
OUTDIR=c:\gs\sh
INTDIR=c:\gs\tmp\sh

ALL : "c:\gs\gdb-sh.exe" $(OUTDIR)/gui.bsc

$(OUTDIR) : 
    if not exist $(OUTDIR)/nul mkdir $(OUTDIR)

$(INTDIR) : 
    if not exist $(INTDIR)/nul mkdir $(INTDIR)

MTL_PROJ=
# ADD BASE CPP /nologo /G4 /MD /W3 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I "g:\gdb\mswin\prebuilt\sh" /I "prebuilt" /I "..\..\mmalloc" /I "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_AFXDLL" /D "_MBCS" /D "TARGET_SH" /FR /c
# ADD CPP /nologo /G4 /MT /W3 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I "g:\gdb\mswin\prebuilt\sh" /I "prebuilt" /I "..\..\mmalloc" /I "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_SH" /D "_MBCS" /D "NEED_basename" /FR /c
CPP_PROJ=/nologo /G4 /MT /W3 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt\sh" /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_SH" /D "_MBCS" /D "NEED_basename"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c 
CPP_OBJS=c:\gs\tmp\sh/
# ADD BASE RSC /l 0x409 /d "_DEBUG" /d "_AFXDLL"
# ADD RSC /l 0x409 /d "_DEBUG"
RSC_PROJ=/l 0x409 /fo$(INTDIR)/"gui.res" /d "_DEBUG" 
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o$(OUTDIR)/"gui.bsc" 
BSC32_SBRS= \
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
	$(INTDIR)/"ser-win32s.sbr" \
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
	$(INTDIR)/"cplus-dem.sbr" \
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
	$(INTDIR)/"dis-buf.sbr" \
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
	$(INTDIR)/stabs.sbr \
	$(INTDIR)/"stab-syms.sbr" \
	$(INTDIR)/annotate.sbr \
	$(INTDIR)/blockframe.sbr \
	$(INTDIR)/breakpoint.sbr \
	$(INTDIR)/buildsym.sbr \
	$(INTDIR)/"c-lang.sbr" \
	$(INTDIR)/"c-typeprint.sbr" \
	$(INTDIR)/"c-valprint.sbr" \
	$(INTDIR)/cexptab.sbr \
	$(INTDIR)/"ch-lang.sbr" \
	$(INTDIR)/"ch-typeprint.sbr" \
	$(INTDIR)/"ch-valprint.sbr" \
	$(INTDIR)/coffread.sbr \
	$(INTDIR)/command.sbr \
	$(INTDIR)/complaints.sbr \
	$(INTDIR)/copying.sbr \
	$(INTDIR)/corefile.sbr \
	$(INTDIR)/"cp-valprint.sbr" \
	$(INTDIR)/dbxread.sbr \
	$(INTDIR)/dcache.sbr \
	$(INTDIR)/demangle.sbr \
	$(INTDIR)/dwarfread.sbr \
	$(INTDIR)/elfread.sbr \
	$(INTDIR)/environ.sbr \
	$(INTDIR)/eval.sbr \
	$(INTDIR)/exec.sbr \
	$(INTDIR)/expprint.sbr \
	$(INTDIR)/"f-lang.sbr" \
	$(INTDIR)/"f-typeprint.sbr" \
	$(INTDIR)/"f-valprint.sbr" \
	$(INTDIR)/fexptab.sbr \
	$(INTDIR)/findvar.sbr \
	$(INTDIR)/gdbtypes.sbr \
	$(INTDIR)/infcmd.sbr \
	$(INTDIR)/infrun.sbr \
	$(INTDIR)/language.sbr \
	$(INTDIR)/"m2-lang.sbr" \
	$(INTDIR)/"m2-typeprint.sbr" \
	$(INTDIR)/"m2-valprint.sbr" \
	$(INTDIR)/m2exptab.sbr \
	$(INTDIR)/main.sbr \
	$(INTDIR)/maint.sbr \
	$(INTDIR)/mdebugread.sbr \
	$(INTDIR)/"mem-break.sbr" \
	$(INTDIR)/minsyms.sbr \
	$(INTDIR)/objfiles.sbr \
	$(INTDIR)/parse.sbr \
	$(INTDIR)/printcmd.sbr \
	$(INTDIR)/"remote-utils.sbr" \
	$(INTDIR)/remote.sbr \
	$(INTDIR)/serial.sbr \
	$(INTDIR)/source.sbr \
	$(INTDIR)/stabsread.sbr \
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
	$(INTDIR)/"scm-lang.sbr" \
	$(INTDIR)/"scm-exp.sbr" \
	$(INTDIR)/"scm-valprint.sbr" \
	$(INTDIR)/"gnu-regex.sbr" \
	$(INTDIR)/dsrec.sbr \
	$(INTDIR)/parallel.sbr \
	$(INTDIR)/"ch-exp.sbr" \
	$(INTDIR)/bcache.sbr \
	$(INTDIR)/"sh-tdep.sbr" \
	$(INTDIR)/interp.sbr \
	$(INTDIR)/table.sbr \
	$(INTDIR)/"sh-dis.sbr" \
	$(INTDIR)/"cpu-sh.sbr" \
	$(INTDIR)/"coff-sh.sbr" \
	$(INTDIR)/"remote-sim.sbr" \
	$(INTDIR)/version.sbr \
	$(INTDIR)/"sh3-rom.sbr" \
	$(INTDIR)/"remote-e7000.sbr" \
	$(INTDIR)/"ser-e7kpc.sbr"

$(OUTDIR)/gui.bsc : $(OUTDIR)  $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
# ADD BASE LINK32 /NOLOGO /SUBSYSTEM:windows /DEBUG /MACHINE:I386
# SUBTRACT BASE LINK32 /PDB:none
# ADD LINK32 /NOLOGO /SUBSYSTEM:windows /DEBUG /MACHINE:I386 /OUT:"c:\gs\gdb-sh.exe"
# SUBTRACT LINK32 /VERBOSE /PDB:none
LINK32_FLAGS=/NOLOGO /SUBSYSTEM:windows /INCREMENTAL:yes\
 /PDB:$(OUTDIR)/"gui.pdb" /DEBUG /MACHINE:I386 /OUT:"c:\gs\gdb-sh.exe" 
DEF_FILE=
LINK32_OBJS= \
	$(INTDIR)/gui.obj \
	$(INTDIR)/mainfrm.obj \
	$(INTDIR)/stdafx.obj \
	$(INTDIR)/aboutbox.obj \
	$(INTDIR)/iface.obj \
	$(INTDIR)/fontinfo.obj \
	$(INTDIR)/regview.obj \
	$(INTDIR)/regdoc.obj \
	$(INTDIR)/gui.res \
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
	.\serdll32.lib \
	$(INTDIR)/mem.obj \
	$(INTDIR)/bfdcore.obj \
	$(INTDIR)/change.obj \
	$(INTDIR)/frameview.obj \
	$(INTDIR)/log.obj \
	$(INTDIR)/mini.obj \
	$(INTDIR)/option.obj \
	$(INTDIR)/"ser-win32s.obj" \
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
	$(INTDIR)/"cplus-dem.obj" \
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
	$(INTDIR)/"dis-buf.obj" \
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
	$(INTDIR)/stabs.obj \
	$(INTDIR)/"stab-syms.obj" \
	$(INTDIR)/annotate.obj \
	$(INTDIR)/blockframe.obj \
	$(INTDIR)/breakpoint.obj \
	$(INTDIR)/buildsym.obj \
	$(INTDIR)/"c-lang.obj" \
	$(INTDIR)/"c-typeprint.obj" \
	$(INTDIR)/"c-valprint.obj" \
	$(INTDIR)/cexptab.obj \
	$(INTDIR)/"ch-lang.obj" \
	$(INTDIR)/"ch-typeprint.obj" \
	$(INTDIR)/"ch-valprint.obj" \
	$(INTDIR)/coffread.obj \
	$(INTDIR)/command.obj \
	$(INTDIR)/complaints.obj \
	$(INTDIR)/copying.obj \
	$(INTDIR)/corefile.obj \
	$(INTDIR)/"cp-valprint.obj" \
	$(INTDIR)/dbxread.obj \
	$(INTDIR)/dcache.obj \
	$(INTDIR)/demangle.obj \
	$(INTDIR)/dwarfread.obj \
	$(INTDIR)/elfread.obj \
	$(INTDIR)/environ.obj \
	$(INTDIR)/eval.obj \
	$(INTDIR)/exec.obj \
	$(INTDIR)/expprint.obj \
	$(INTDIR)/"f-lang.obj" \
	$(INTDIR)/"f-typeprint.obj" \
	$(INTDIR)/"f-valprint.obj" \
	$(INTDIR)/fexptab.obj \
	$(INTDIR)/findvar.obj \
	$(INTDIR)/gdbtypes.obj \
	$(INTDIR)/infcmd.obj \
	$(INTDIR)/infrun.obj \
	$(INTDIR)/language.obj \
	$(INTDIR)/"m2-lang.obj" \
	$(INTDIR)/"m2-typeprint.obj" \
	$(INTDIR)/"m2-valprint.obj" \
	$(INTDIR)/m2exptab.obj \
	$(INTDIR)/main.obj \
	$(INTDIR)/maint.obj \
	$(INTDIR)/mdebugread.obj \
	$(INTDIR)/"mem-break.obj" \
	$(INTDIR)/minsyms.obj \
	$(INTDIR)/objfiles.obj \
	$(INTDIR)/parse.obj \
	$(INTDIR)/printcmd.obj \
	$(INTDIR)/"remote-utils.obj" \
	$(INTDIR)/remote.obj \
	$(INTDIR)/serial.obj \
	$(INTDIR)/source.obj \
	$(INTDIR)/stabsread.obj \
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
	$(INTDIR)/"scm-lang.obj" \
	$(INTDIR)/"scm-exp.obj" \
	$(INTDIR)/"scm-valprint.obj" \
	$(INTDIR)/"gnu-regex.obj" \
	$(INTDIR)/dsrec.obj \
	$(INTDIR)/parallel.obj \
	$(INTDIR)/"ch-exp.obj" \
	$(INTDIR)/bcache.obj \
	$(INTDIR)/"sh-tdep.obj" \
	$(INTDIR)/interp.obj \
	$(INTDIR)/table.obj \
	$(INTDIR)/"sh-dis.obj" \
	$(INTDIR)/"cpu-sh.obj" \
	$(INTDIR)/"coff-sh.obj" \
	$(INTDIR)/"remote-sim.obj" \
	$(INTDIR)/version.obj \
	$(INTDIR)/"sh3-rom.obj" \
	$(INTDIR)/"remote-e7000.obj" \
	$(INTDIR)/"ser-e7kpc.obj"

"c:\gs\gdb-sh.exe" : $(OUTDIR)  $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "Win32 Release"

# PROP BASE Use_MFC 6
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "WinRel"
# PROP BASE Intermediate_Dir "WinRel"
# PROP Use_MFC 5
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "c:\guinodeb"
# PROP Intermediate_Dir "c:\guinodeb"
OUTDIR=c:\guinodeb
INTDIR=c:\guinodeb

ALL : c:\demo\gdbnodeb.exe $(OUTDIR)/gui.bsc

$(OUTDIR) : 
    if not exist $(OUTDIR)/nul mkdir $(OUTDIR)

MTL_PROJ=
# ADD BASE CPP /nologo /MD /W3 /GX /O2 /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_AFXDLL" /FR /Yu"stdafx.h" /c
# ADD CPP /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR /c
# SUBTRACT CPP /WX
CPP_PROJ=/nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c 
CPP_OBJS=c:\guinodeb/
# ADD BASE RSC /l 0x409 /d "NDEBUG" /d "_AFXDLL"
# ADD RSC /l 0x409 /d "NDEBUG"
RSC_PROJ=/l 0x409 /fo$(INTDIR)/"gui.res" /d "NDEBUG" 
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o$(OUTDIR)/"gui.bsc" 
BSC32_SBRS= \
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
	$(INTDIR)/"ser-win32s.sbr" \
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
	$(INTDIR)/"cplus-dem.sbr" \
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
	$(INTDIR)/"dis-buf.sbr" \
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
	$(INTDIR)/stabs.sbr \
	$(INTDIR)/"stab-syms.sbr" \
	$(INTDIR)/annotate.sbr \
	$(INTDIR)/blockframe.sbr \
	$(INTDIR)/breakpoint.sbr \
	$(INTDIR)/buildsym.sbr \
	$(INTDIR)/"c-lang.sbr" \
	$(INTDIR)/"c-typeprint.sbr" \
	$(INTDIR)/"c-valprint.sbr" \
	$(INTDIR)/cexptab.sbr \
	$(INTDIR)/"ch-lang.sbr" \
	$(INTDIR)/"ch-typeprint.sbr" \
	$(INTDIR)/"ch-valprint.sbr" \
	$(INTDIR)/coffread.sbr \
	$(INTDIR)/command.sbr \
	$(INTDIR)/complaints.sbr \
	$(INTDIR)/copying.sbr \
	$(INTDIR)/corefile.sbr \
	$(INTDIR)/"cp-valprint.sbr" \
	$(INTDIR)/dbxread.sbr \
	$(INTDIR)/dcache.sbr \
	$(INTDIR)/demangle.sbr \
	$(INTDIR)/dwarfread.sbr \
	$(INTDIR)/elfread.sbr \
	$(INTDIR)/environ.sbr \
	$(INTDIR)/eval.sbr \
	$(INTDIR)/exec.sbr \
	$(INTDIR)/expprint.sbr \
	$(INTDIR)/"f-lang.sbr" \
	$(INTDIR)/"f-typeprint.sbr" \
	$(INTDIR)/"f-valprint.sbr" \
	$(INTDIR)/fexptab.sbr \
	$(INTDIR)/findvar.sbr \
	$(INTDIR)/gdbtypes.sbr \
	$(INTDIR)/infcmd.sbr \
	$(INTDIR)/infrun.sbr \
	$(INTDIR)/language.sbr \
	$(INTDIR)/"m2-lang.sbr" \
	$(INTDIR)/"m2-typeprint.sbr" \
	$(INTDIR)/"m2-valprint.sbr" \
	$(INTDIR)/m2exptab.sbr \
	$(INTDIR)/main.sbr \
	$(INTDIR)/maint.sbr \
	$(INTDIR)/mdebugread.sbr \
	$(INTDIR)/"mem-break.sbr" \
	$(INTDIR)/minsyms.sbr \
	$(INTDIR)/objfiles.sbr \
	$(INTDIR)/parse.sbr \
	$(INTDIR)/printcmd.sbr \
	$(INTDIR)/"remote-utils.sbr" \
	$(INTDIR)/remote.sbr \
	$(INTDIR)/serial.sbr \
	$(INTDIR)/source.sbr \
	$(INTDIR)/stabsread.sbr \
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
	$(INTDIR)/"scm-lang.sbr" \
	$(INTDIR)/"scm-exp.sbr" \
	$(INTDIR)/"scm-valprint.sbr" \
	$(INTDIR)/"gnu-regex.sbr" \
	$(INTDIR)/dsrec.sbr \
	$(INTDIR)/parallel.sbr \
	$(INTDIR)/"ch-exp.sbr" \
	$(INTDIR)/bcache.sbr \
	$(INTDIR)/"sh-tdep.sbr" \
	$(INTDIR)/interp.sbr \
	$(INTDIR)/table.sbr \
	$(INTDIR)/"sh-dis.sbr" \
	$(INTDIR)/"cpu-sh.sbr" \
	$(INTDIR)/"coff-sh.sbr" \
	$(INTDIR)/"remote-sim.sbr" \
	$(INTDIR)/version.sbr \
	$(INTDIR)/"sh3-rom.sbr" \
	$(INTDIR)/"remote-e7000.sbr" \
	$(INTDIR)/"ser-e7kpc.sbr" \
	$(INTDIR)/"cpu-h8300.sbr" \
	$(INTDIR)/compile.sbr \
	$(INTDIR)/"coff-h8300.sbr" \
	$(INTDIR)/"h8300-tdep.sbr" \
	$(INTDIR)/reloc16.sbr \
	$(INTDIR)/"h8300-dis.sbr" \
	$(INTDIR)/"remote-hms.sbr" \
	$(INTDIR)/"coff-m68k.sbr" \
	$(INTDIR)/"m68k-dis.sbr" \
	$(INTDIR)/"m68k-tdep.sbr" \
	$(INTDIR)/"remote-est.sbr" \
	$(INTDIR)/"cpu-m68k.sbr" \
	$(INTDIR)/aout0.sbr \
	$(INTDIR)/aout32.sbr \
	$(INTDIR)/"cpu32bug-rom.sbr" \
	$(INTDIR)/"rom68k-rom.sbr" \
	$(INTDIR)/"m68k-opc.sbr" \
	$(INTDIR)/"coff-sparc.sbr" \
	$(INTDIR)/"sparc-tdep.sbr" \
	$(INTDIR)/"sparcl-tdep.sbr" \
	$(INTDIR)/"sparc-opc.sbr" \
	$(INTDIR)/"sparc-dis.sbr" \
	$(INTDIR)/"cpu-sparc.sbr" \
	$(INTDIR)/sunos.sbr \
	$(INTDIR)/"remote-mips.sbr" \
	$(INTDIR)/"elf32-mips.sbr" \
	$(INTDIR)/elf.sbr \
	$(INTDIR)/elf32.sbr \
	$(INTDIR)/"cpu-mips.sbr" \
	$(INTDIR)/"mips-tdep.sbr" \
	$(INTDIR)/ecofflink.sbr \
	$(INTDIR)/"coff-mips.sbr" \
	$(INTDIR)/ecoff.sbr \
	$(INTDIR)/"mips-opc.sbr" \
	$(INTDIR)/"mips-dis.sbr" \
	$(INTDIR)/elflink.sbr \
	$(INTDIR)/"a29k-tdep.sbr" \
	$(INTDIR)/"coff-a29k.sbr" \
	$(INTDIR)/"cpu-a29k.sbr" \
	$(INTDIR)/udi2go32.sbr \
	$(INTDIR)/"remote-udi.sbr" \
	$(INTDIR)/udr.sbr \
	$(INTDIR)/udip2soc.sbr \
	$(INTDIR)/"a29k-dis.sbr" \
	$(INTDIR)/"cpu-i386.sbr" \
	$(INTDIR)/"i386-tdep.sbr" \
	$(INTDIR)/"i386-dis.sbr" \
	$(INTDIR)/i386aout.sbr

$(OUTDIR)/gui.bsc : $(OUTDIR)  $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
# ADD BASE LINK32 /NOLOGO /SUBSYSTEM:windows /MACHINE:I386
# SUBTRACT BASE LINK32 /PDB:none
# ADD LINK32 /NOLOGO /SUBSYSTEM:windows /MACHINE:I386 /OUT:"c:\demo\gdbnodeb.exe"
# SUBTRACT LINK32 /VERBOSE /PDB:none
LINK32_FLAGS=/NOLOGO /SUBSYSTEM:windows /INCREMENTAL:no\
 /PDB:$(OUTDIR)/"gui.pdb" /MACHINE:I386 /OUT:"c:\demo\gdbnodeb.exe" 
DEF_FILE=
LINK32_OBJS= \
	$(INTDIR)/gui.obj \
	$(INTDIR)/mainfrm.obj \
	$(INTDIR)/stdafx.obj \
	$(INTDIR)/aboutbox.obj \
	$(INTDIR)/iface.obj \
	$(INTDIR)/fontinfo.obj \
	$(INTDIR)/regview.obj \
	$(INTDIR)/regdoc.obj \
	$(INTDIR)/gui.res \
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
	.\serdll32.lib \
	$(INTDIR)/mem.obj \
	$(INTDIR)/bfdcore.obj \
	$(INTDIR)/change.obj \
	$(INTDIR)/frameview.obj \
	$(INTDIR)/log.obj \
	$(INTDIR)/mini.obj \
	$(INTDIR)/option.obj \
	$(INTDIR)/"ser-win32s.obj" \
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
	$(INTDIR)/"cplus-dem.obj" \
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
	$(INTDIR)/"dis-buf.obj" \
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
	$(INTDIR)/stabs.obj \
	$(INTDIR)/"stab-syms.obj" \
	$(INTDIR)/annotate.obj \
	$(INTDIR)/blockframe.obj \
	$(INTDIR)/breakpoint.obj \
	$(INTDIR)/buildsym.obj \
	$(INTDIR)/"c-lang.obj" \
	$(INTDIR)/"c-typeprint.obj" \
	$(INTDIR)/"c-valprint.obj" \
	$(INTDIR)/cexptab.obj \
	$(INTDIR)/"ch-lang.obj" \
	$(INTDIR)/"ch-typeprint.obj" \
	$(INTDIR)/"ch-valprint.obj" \
	$(INTDIR)/coffread.obj \
	$(INTDIR)/command.obj \
	$(INTDIR)/complaints.obj \
	$(INTDIR)/copying.obj \
	$(INTDIR)/corefile.obj \
	$(INTDIR)/"cp-valprint.obj" \
	$(INTDIR)/dbxread.obj \
	$(INTDIR)/dcache.obj \
	$(INTDIR)/demangle.obj \
	$(INTDIR)/dwarfread.obj \
	$(INTDIR)/elfread.obj \
	$(INTDIR)/environ.obj \
	$(INTDIR)/eval.obj \
	$(INTDIR)/exec.obj \
	$(INTDIR)/expprint.obj \
	$(INTDIR)/"f-lang.obj" \
	$(INTDIR)/"f-typeprint.obj" \
	$(INTDIR)/"f-valprint.obj" \
	$(INTDIR)/fexptab.obj \
	$(INTDIR)/findvar.obj \
	$(INTDIR)/gdbtypes.obj \
	$(INTDIR)/infcmd.obj \
	$(INTDIR)/infrun.obj \
	$(INTDIR)/language.obj \
	$(INTDIR)/"m2-lang.obj" \
	$(INTDIR)/"m2-typeprint.obj" \
	$(INTDIR)/"m2-valprint.obj" \
	$(INTDIR)/m2exptab.obj \
	$(INTDIR)/main.obj \
	$(INTDIR)/maint.obj \
	$(INTDIR)/mdebugread.obj \
	$(INTDIR)/"mem-break.obj" \
	$(INTDIR)/minsyms.obj \
	$(INTDIR)/objfiles.obj \
	$(INTDIR)/parse.obj \
	$(INTDIR)/printcmd.obj \
	$(INTDIR)/"remote-utils.obj" \
	$(INTDIR)/remote.obj \
	$(INTDIR)/serial.obj \
	$(INTDIR)/source.obj \
	$(INTDIR)/stabsread.obj \
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
	$(INTDIR)/"scm-lang.obj" \
	$(INTDIR)/"scm-exp.obj" \
	$(INTDIR)/"scm-valprint.obj" \
	$(INTDIR)/"gnu-regex.obj" \
	$(INTDIR)/dsrec.obj \
	$(INTDIR)/parallel.obj \
	$(INTDIR)/"ch-exp.obj" \
	$(INTDIR)/bcache.obj \
	$(INTDIR)/"sh-tdep.obj" \
	$(INTDIR)/interp.obj \
	$(INTDIR)/table.obj \
	$(INTDIR)/"sh-dis.obj" \
	$(INTDIR)/"cpu-sh.obj" \
	$(INTDIR)/"coff-sh.obj" \
	$(INTDIR)/"remote-sim.obj" \
	$(INTDIR)/version.obj \
	$(INTDIR)/"sh3-rom.obj" \
	$(INTDIR)/"remote-e7000.obj" \
	$(INTDIR)/"ser-e7kpc.obj" \
	$(INTDIR)/"cpu-h8300.obj" \
	$(INTDIR)/compile.obj \
	$(INTDIR)/"coff-h8300.obj" \
	$(INTDIR)/"h8300-tdep.obj" \
	$(INTDIR)/reloc16.obj \
	$(INTDIR)/"h8300-dis.obj" \
	$(INTDIR)/"remote-hms.obj" \
	$(INTDIR)/"coff-m68k.obj" \
	$(INTDIR)/"m68k-dis.obj" \
	$(INTDIR)/"m68k-tdep.obj" \
	$(INTDIR)/"remote-est.obj" \
	$(INTDIR)/"cpu-m68k.obj" \
	$(INTDIR)/aout0.obj \
	$(INTDIR)/aout32.obj \
	$(INTDIR)/"cpu32bug-rom.obj" \
	$(INTDIR)/"rom68k-rom.obj" \
	$(INTDIR)/"m68k-opc.obj" \
	$(INTDIR)/"coff-sparc.obj" \
	$(INTDIR)/"sparc-tdep.obj" \
	$(INTDIR)/"sparcl-tdep.obj" \
	$(INTDIR)/"sparc-opc.obj" \
	$(INTDIR)/"sparc-dis.obj" \
	$(INTDIR)/"cpu-sparc.obj" \
	$(INTDIR)/sunos.obj \
	$(INTDIR)/"remote-mips.obj" \
	$(INTDIR)/"elf32-mips.obj" \
	$(INTDIR)/elf.obj \
	$(INTDIR)/elf32.obj \
	$(INTDIR)/"cpu-mips.obj" \
	$(INTDIR)/"mips-tdep.obj" \
	$(INTDIR)/ecofflink.obj \
	$(INTDIR)/"coff-mips.obj" \
	$(INTDIR)/ecoff.obj \
	$(INTDIR)/"mips-opc.obj" \
	$(INTDIR)/"mips-dis.obj" \
	$(INTDIR)/elflink.obj \
	$(INTDIR)/"a29k-tdep.obj" \
	$(INTDIR)/"coff-a29k.obj" \
	$(INTDIR)/"cpu-a29k.obj" \
	$(INTDIR)/udi2go32.obj \
	$(INTDIR)/"remote-udi.obj" \
	$(INTDIR)/udr.obj \
	$(INTDIR)/udip2soc.obj \
	$(INTDIR)/"a29k-dis.obj" \
	$(INTDIR)/"cpu-i386.obj" \
	$(INTDIR)/"i386-tdep.obj" \
	$(INTDIR)/"i386-dis.obj" \
	$(INTDIR)/i386aout.obj

c:\demo\gdbnodeb.exe : $(OUTDIR)  $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "h8300"

# PROP BASE Use_MFC 6
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "h8300"
# PROP BASE Intermediate_Dir "h8300"
# PROP Use_MFC 5
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "c:\gs\h8300"
# PROP Intermediate_Dir "c:\gs\tmp\h8300"
OUTDIR=c:\gs\h8300
INTDIR=c:\gs\tmp\h8300

ALL : "c:\gs\gdb-h83.exe" $(OUTDIR)/gui.bsc

$(OUTDIR) : 
    if not exist $(OUTDIR)/nul mkdir $(OUTDIR)

$(INTDIR) : 
    if not exist $(INTDIR)/nul mkdir $(INTDIR)

MTL_PROJ=
# ADD BASE CPP /nologo /G4 /MD /W3 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I "g:\gdb\mswin\prebuilt\sh" /I "prebuilt" /I "..\..\mmalloc" /I "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_AFXDLL" /D "_MBCS" /D "TARGET_SH" /FR /c
# ADD CPP /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I "g:\gdb\mswin\prebuilt" /I "g:\gdb\mswin\prebuilt\h8300" /I "..\..\mmalloc" /I "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_H8300" /D "_MBCS" /D "NEED_basename" /Fr /c
# SUBTRACT CPP /WX
CPP_PROJ=/nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt" /I "g:\gdb\mswin\prebuilt\h8300" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_H8300" /D "_MBCS" /D "NEED_basename"\
 /Fr$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c 
CPP_OBJS=c:\gs\tmp\h8300/
# ADD BASE RSC /l 0x409 /d "_DEBUG" /d "_AFXDLL"
# ADD RSC /l 0x409 /d "_DEBUG"
RSC_PROJ=/l 0x409 /fo$(INTDIR)/"gui.res" /d "_DEBUG" 
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o$(OUTDIR)/"gui.bsc" 
BSC32_SBRS= \
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
	$(INTDIR)/"ser-win32s.sbr" \
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
	$(INTDIR)/"cplus-dem.sbr" \
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
	$(INTDIR)/"dis-buf.sbr" \
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
	$(INTDIR)/stabs.sbr \
	$(INTDIR)/"stab-syms.sbr" \
	$(INTDIR)/annotate.sbr \
	$(INTDIR)/blockframe.sbr \
	$(INTDIR)/breakpoint.sbr \
	$(INTDIR)/buildsym.sbr \
	$(INTDIR)/"c-lang.sbr" \
	$(INTDIR)/"c-typeprint.sbr" \
	$(INTDIR)/"c-valprint.sbr" \
	$(INTDIR)/cexptab.sbr \
	$(INTDIR)/"ch-lang.sbr" \
	$(INTDIR)/"ch-typeprint.sbr" \
	$(INTDIR)/"ch-valprint.sbr" \
	$(INTDIR)/coffread.sbr \
	$(INTDIR)/command.sbr \
	$(INTDIR)/complaints.sbr \
	$(INTDIR)/copying.sbr \
	$(INTDIR)/corefile.sbr \
	$(INTDIR)/"cp-valprint.sbr" \
	$(INTDIR)/dbxread.sbr \
	$(INTDIR)/dcache.sbr \
	$(INTDIR)/demangle.sbr \
	$(INTDIR)/dwarfread.sbr \
	$(INTDIR)/elfread.sbr \
	$(INTDIR)/environ.sbr \
	$(INTDIR)/eval.sbr \
	$(INTDIR)/exec.sbr \
	$(INTDIR)/expprint.sbr \
	$(INTDIR)/"f-lang.sbr" \
	$(INTDIR)/"f-typeprint.sbr" \
	$(INTDIR)/"f-valprint.sbr" \
	$(INTDIR)/fexptab.sbr \
	$(INTDIR)/findvar.sbr \
	$(INTDIR)/gdbtypes.sbr \
	$(INTDIR)/infcmd.sbr \
	$(INTDIR)/infrun.sbr \
	$(INTDIR)/language.sbr \
	$(INTDIR)/"m2-lang.sbr" \
	$(INTDIR)/"m2-typeprint.sbr" \
	$(INTDIR)/"m2-valprint.sbr" \
	$(INTDIR)/m2exptab.sbr \
	$(INTDIR)/main.sbr \
	$(INTDIR)/maint.sbr \
	$(INTDIR)/mdebugread.sbr \
	$(INTDIR)/"mem-break.sbr" \
	$(INTDIR)/minsyms.sbr \
	$(INTDIR)/objfiles.sbr \
	$(INTDIR)/parse.sbr \
	$(INTDIR)/printcmd.sbr \
	$(INTDIR)/"remote-utils.sbr" \
	$(INTDIR)/remote.sbr \
	$(INTDIR)/serial.sbr \
	$(INTDIR)/source.sbr \
	$(INTDIR)/stabsread.sbr \
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
	$(INTDIR)/"scm-lang.sbr" \
	$(INTDIR)/"scm-exp.sbr" \
	$(INTDIR)/"scm-valprint.sbr" \
	$(INTDIR)/"gnu-regex.sbr" \
	$(INTDIR)/dsrec.sbr \
	$(INTDIR)/parallel.sbr \
	$(INTDIR)/"ch-exp.sbr" \
	$(INTDIR)/bcache.sbr \
	$(INTDIR)/"cpu-h8300.sbr" \
	$(INTDIR)/compile.sbr \
	$(INTDIR)/"coff-h8300.sbr" \
	$(INTDIR)/"h8300-tdep.sbr" \
	$(INTDIR)/reloc16.sbr \
	$(INTDIR)/"h8300-dis.sbr" \
	$(INTDIR)/"remote-hms.sbr" \
	$(INTDIR)/"remote-sim.sbr" \
	$(INTDIR)/version.sbr \
	$(INTDIR)/"remote-e7000.sbr" \
	$(INTDIR)/"ser-e7kpc.sbr"

$(OUTDIR)/gui.bsc : $(OUTDIR)  $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
# ADD BASE LINK32 /NOLOGO /SUBSYSTEM:windows /DEBUG /MACHINE:I386 /OUT:"c:\demo\gdb.exe"
# SUBTRACT BASE LINK32 /VERBOSE /PDB:none
# ADD LINK32 /NOLOGO /SUBSYSTEM:windows /DEBUG /MACHINE:I386 /OUT:"c:\gs\gdb-h83.exe"
# SUBTRACT LINK32 /VERBOSE /PDB:none
LINK32_FLAGS=/NOLOGO /SUBSYSTEM:windows /INCREMENTAL:yes\
 /PDB:$(OUTDIR)/"gui.pdb" /DEBUG /MACHINE:I386 /OUT:"c:\gs\gdb-h83.exe" 
DEF_FILE=
LINK32_OBJS= \
	$(INTDIR)/gui.obj \
	$(INTDIR)/mainfrm.obj \
	$(INTDIR)/stdafx.obj \
	$(INTDIR)/aboutbox.obj \
	$(INTDIR)/iface.obj \
	$(INTDIR)/fontinfo.obj \
	$(INTDIR)/regview.obj \
	$(INTDIR)/regdoc.obj \
	$(INTDIR)/gui.res \
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
	.\serdll32.lib \
	$(INTDIR)/mem.obj \
	$(INTDIR)/bfdcore.obj \
	$(INTDIR)/change.obj \
	$(INTDIR)/frameview.obj \
	$(INTDIR)/log.obj \
	$(INTDIR)/mini.obj \
	$(INTDIR)/option.obj \
	$(INTDIR)/"ser-win32s.obj" \
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
	$(INTDIR)/"cplus-dem.obj" \
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
	$(INTDIR)/"dis-buf.obj" \
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
	$(INTDIR)/stabs.obj \
	$(INTDIR)/"stab-syms.obj" \
	$(INTDIR)/annotate.obj \
	$(INTDIR)/blockframe.obj \
	$(INTDIR)/breakpoint.obj \
	$(INTDIR)/buildsym.obj \
	$(INTDIR)/"c-lang.obj" \
	$(INTDIR)/"c-typeprint.obj" \
	$(INTDIR)/"c-valprint.obj" \
	$(INTDIR)/cexptab.obj \
	$(INTDIR)/"ch-lang.obj" \
	$(INTDIR)/"ch-typeprint.obj" \
	$(INTDIR)/"ch-valprint.obj" \
	$(INTDIR)/coffread.obj \
	$(INTDIR)/command.obj \
	$(INTDIR)/complaints.obj \
	$(INTDIR)/copying.obj \
	$(INTDIR)/corefile.obj \
	$(INTDIR)/"cp-valprint.obj" \
	$(INTDIR)/dbxread.obj \
	$(INTDIR)/dcache.obj \
	$(INTDIR)/demangle.obj \
	$(INTDIR)/dwarfread.obj \
	$(INTDIR)/elfread.obj \
	$(INTDIR)/environ.obj \
	$(INTDIR)/eval.obj \
	$(INTDIR)/exec.obj \
	$(INTDIR)/expprint.obj \
	$(INTDIR)/"f-lang.obj" \
	$(INTDIR)/"f-typeprint.obj" \
	$(INTDIR)/"f-valprint.obj" \
	$(INTDIR)/fexptab.obj \
	$(INTDIR)/findvar.obj \
	$(INTDIR)/gdbtypes.obj \
	$(INTDIR)/infcmd.obj \
	$(INTDIR)/infrun.obj \
	$(INTDIR)/language.obj \
	$(INTDIR)/"m2-lang.obj" \
	$(INTDIR)/"m2-typeprint.obj" \
	$(INTDIR)/"m2-valprint.obj" \
	$(INTDIR)/m2exptab.obj \
	$(INTDIR)/main.obj \
	$(INTDIR)/maint.obj \
	$(INTDIR)/mdebugread.obj \
	$(INTDIR)/"mem-break.obj" \
	$(INTDIR)/minsyms.obj \
	$(INTDIR)/objfiles.obj \
	$(INTDIR)/parse.obj \
	$(INTDIR)/printcmd.obj \
	$(INTDIR)/"remote-utils.obj" \
	$(INTDIR)/remote.obj \
	$(INTDIR)/serial.obj \
	$(INTDIR)/source.obj \
	$(INTDIR)/stabsread.obj \
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
	$(INTDIR)/"scm-lang.obj" \
	$(INTDIR)/"scm-exp.obj" \
	$(INTDIR)/"scm-valprint.obj" \
	$(INTDIR)/"gnu-regex.obj" \
	$(INTDIR)/dsrec.obj \
	$(INTDIR)/parallel.obj \
	$(INTDIR)/"ch-exp.obj" \
	$(INTDIR)/bcache.obj \
	$(INTDIR)/"cpu-h8300.obj" \
	$(INTDIR)/compile.obj \
	$(INTDIR)/"coff-h8300.obj" \
	$(INTDIR)/"h8300-tdep.obj" \
	$(INTDIR)/reloc16.obj \
	$(INTDIR)/"h8300-dis.obj" \
	$(INTDIR)/"remote-hms.obj" \
	$(INTDIR)/"remote-sim.obj" \
	$(INTDIR)/version.obj \
	$(INTDIR)/"remote-e7000.obj" \
	$(INTDIR)/"ser-e7kpc.obj"

"c:\gs\gdb-h83.exe" : $(OUTDIR)  $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "m68k"

# PROP BASE Use_MFC 5
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "est"
# PROP BASE Intermediate_Dir "est"
# PROP Use_MFC 5
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "c:\gs\m68k"
# PROP Intermediate_Dir "c:\gs\tmp\m68k"
OUTDIR=c:\gs\m68k
INTDIR=c:\gs\tmp\m68k

ALL : "c:\gs\gdb-m68k.exe" $(OUTDIR)/gui.bsc

$(OUTDIR) : 
    if not exist $(OUTDIR)/nul mkdir $(OUTDIR)

$(INTDIR) : 
    if not exist $(INTDIR)/nul mkdir $(INTDIR)

MTL_PROJ=
# ADD BASE CPP /nologo /G4 /MT /W3 /GX /YX /O1 /I "g:\gdb\mswin" /I "g:\gdb\mswin\prebuilt\sh" /I "prebuilt" /I "..\..\mmalloc" /I "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_SH" /D "_MBCS" /FR /c
# ADD CPP /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\config" /I "g:\gdb\mswin" /I "g:\gdb\mswin\prebuilt\m68k" /I "prebuilt" /I "..\..\mmalloc" /I "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "__WIN32__" /D "_WINDOWS" /D "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_M68K" /D "_MBCS" /D "TARGET_EST" /Fr /c
# SUBTRACT CPP /WX
CPP_PROJ=/nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\config" /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt\m68k" /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "__WIN32__" /D\
 "_WINDOWS" /D "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_M68K" /D "_MBCS" /D\
 "TARGET_EST" /Fr$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/\
 /Fd$(OUTDIR)/"gui.pdb" /c 
CPP_OBJS=c:\gs\tmp\m68k/
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
RSC_PROJ=/l 0x409 /fo$(INTDIR)/"gui.res" /d "_DEBUG" 
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o$(OUTDIR)/"gui.bsc" 
BSC32_SBRS= \
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
	$(INTDIR)/"ser-win32s.sbr" \
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
	$(INTDIR)/"cplus-dem.sbr" \
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
	$(INTDIR)/"dis-buf.sbr" \
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
	$(INTDIR)/stabs.sbr \
	$(INTDIR)/"stab-syms.sbr" \
	$(INTDIR)/annotate.sbr \
	$(INTDIR)/blockframe.sbr \
	$(INTDIR)/breakpoint.sbr \
	$(INTDIR)/buildsym.sbr \
	$(INTDIR)/"c-lang.sbr" \
	$(INTDIR)/"c-typeprint.sbr" \
	$(INTDIR)/"c-valprint.sbr" \
	$(INTDIR)/cexptab.sbr \
	$(INTDIR)/"ch-lang.sbr" \
	$(INTDIR)/"ch-typeprint.sbr" \
	$(INTDIR)/"ch-valprint.sbr" \
	$(INTDIR)/coffread.sbr \
	$(INTDIR)/command.sbr \
	$(INTDIR)/complaints.sbr \
	$(INTDIR)/copying.sbr \
	$(INTDIR)/corefile.sbr \
	$(INTDIR)/"cp-valprint.sbr" \
	$(INTDIR)/dbxread.sbr \
	$(INTDIR)/dcache.sbr \
	$(INTDIR)/demangle.sbr \
	$(INTDIR)/dwarfread.sbr \
	$(INTDIR)/elfread.sbr \
	$(INTDIR)/environ.sbr \
	$(INTDIR)/eval.sbr \
	$(INTDIR)/exec.sbr \
	$(INTDIR)/expprint.sbr \
	$(INTDIR)/"f-lang.sbr" \
	$(INTDIR)/"f-typeprint.sbr" \
	$(INTDIR)/"f-valprint.sbr" \
	$(INTDIR)/fexptab.sbr \
	$(INTDIR)/findvar.sbr \
	$(INTDIR)/gdbtypes.sbr \
	$(INTDIR)/infcmd.sbr \
	$(INTDIR)/infrun.sbr \
	$(INTDIR)/language.sbr \
	$(INTDIR)/"m2-lang.sbr" \
	$(INTDIR)/"m2-typeprint.sbr" \
	$(INTDIR)/"m2-valprint.sbr" \
	$(INTDIR)/m2exptab.sbr \
	$(INTDIR)/main.sbr \
	$(INTDIR)/maint.sbr \
	$(INTDIR)/mdebugread.sbr \
	$(INTDIR)/"mem-break.sbr" \
	$(INTDIR)/minsyms.sbr \
	$(INTDIR)/objfiles.sbr \
	$(INTDIR)/parse.sbr \
	$(INTDIR)/printcmd.sbr \
	$(INTDIR)/"remote-utils.sbr" \
	$(INTDIR)/remote.sbr \
	$(INTDIR)/serial.sbr \
	$(INTDIR)/source.sbr \
	$(INTDIR)/stabsread.sbr \
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
	$(INTDIR)/"scm-lang.sbr" \
	$(INTDIR)/"scm-exp.sbr" \
	$(INTDIR)/"scm-valprint.sbr" \
	$(INTDIR)/"gnu-regex.sbr" \
	$(INTDIR)/dsrec.sbr \
	$(INTDIR)/parallel.sbr \
	$(INTDIR)/"ch-exp.sbr" \
	$(INTDIR)/bcache.sbr \
	$(INTDIR)/"coff-m68k.sbr" \
	$(INTDIR)/"m68k-dis.sbr" \
	$(INTDIR)/"m68k-tdep.sbr" \
	$(INTDIR)/"remote-est.sbr" \
	$(INTDIR)/"cpu-m68k.sbr" \
	$(INTDIR)/aout0.sbr \
	$(INTDIR)/aout32.sbr \
	$(INTDIR)/"cpu32bug-rom.sbr" \
	$(INTDIR)/"rom68k-rom.sbr" \
	$(INTDIR)/"m68k-opc.sbr" \
	$(INTDIR)/version.sbr

$(OUTDIR)/gui.bsc : $(OUTDIR)  $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
# ADD BASE LINK32 /NOLOGO /SUBSYSTEM:windows /DEBUG /MACHINE:I386 /OUT:"c:\demo\gdb.exe"
# SUBTRACT BASE LINK32 /VERBOSE /PDB:none
# ADD LINK32 /NOLOGO /SUBSYSTEM:windows /DEBUG /MACHINE:I386 /OUT:"c:\gs\gdb-m68k.exe"
# SUBTRACT LINK32 /VERBOSE /PDB:none
LINK32_FLAGS=/NOLOGO /SUBSYSTEM:windows /INCREMENTAL:yes\
 /PDB:$(OUTDIR)/"gui.pdb" /DEBUG /MACHINE:I386 /OUT:"c:\gs\gdb-m68k.exe" 
DEF_FILE=
LINK32_OBJS= \
	$(INTDIR)/gui.obj \
	$(INTDIR)/mainfrm.obj \
	$(INTDIR)/stdafx.obj \
	$(INTDIR)/aboutbox.obj \
	$(INTDIR)/iface.obj \
	$(INTDIR)/fontinfo.obj \
	$(INTDIR)/regview.obj \
	$(INTDIR)/regdoc.obj \
	$(INTDIR)/gui.res \
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
	.\serdll32.lib \
	$(INTDIR)/mem.obj \
	$(INTDIR)/bfdcore.obj \
	$(INTDIR)/change.obj \
	$(INTDIR)/frameview.obj \
	$(INTDIR)/log.obj \
	$(INTDIR)/mini.obj \
	$(INTDIR)/option.obj \
	$(INTDIR)/"ser-win32s.obj" \
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
	$(INTDIR)/"cplus-dem.obj" \
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
	$(INTDIR)/"dis-buf.obj" \
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
	$(INTDIR)/stabs.obj \
	$(INTDIR)/"stab-syms.obj" \
	$(INTDIR)/annotate.obj \
	$(INTDIR)/blockframe.obj \
	$(INTDIR)/breakpoint.obj \
	$(INTDIR)/buildsym.obj \
	$(INTDIR)/"c-lang.obj" \
	$(INTDIR)/"c-typeprint.obj" \
	$(INTDIR)/"c-valprint.obj" \
	$(INTDIR)/cexptab.obj \
	$(INTDIR)/"ch-lang.obj" \
	$(INTDIR)/"ch-typeprint.obj" \
	$(INTDIR)/"ch-valprint.obj" \
	$(INTDIR)/coffread.obj \
	$(INTDIR)/command.obj \
	$(INTDIR)/complaints.obj \
	$(INTDIR)/copying.obj \
	$(INTDIR)/corefile.obj \
	$(INTDIR)/"cp-valprint.obj" \
	$(INTDIR)/dbxread.obj \
	$(INTDIR)/dcache.obj \
	$(INTDIR)/demangle.obj \
	$(INTDIR)/dwarfread.obj \
	$(INTDIR)/elfread.obj \
	$(INTDIR)/environ.obj \
	$(INTDIR)/eval.obj \
	$(INTDIR)/exec.obj \
	$(INTDIR)/expprint.obj \
	$(INTDIR)/"f-lang.obj" \
	$(INTDIR)/"f-typeprint.obj" \
	$(INTDIR)/"f-valprint.obj" \
	$(INTDIR)/fexptab.obj \
	$(INTDIR)/findvar.obj \
	$(INTDIR)/gdbtypes.obj \
	$(INTDIR)/infcmd.obj \
	$(INTDIR)/infrun.obj \
	$(INTDIR)/language.obj \
	$(INTDIR)/"m2-lang.obj" \
	$(INTDIR)/"m2-typeprint.obj" \
	$(INTDIR)/"m2-valprint.obj" \
	$(INTDIR)/m2exptab.obj \
	$(INTDIR)/main.obj \
	$(INTDIR)/maint.obj \
	$(INTDIR)/mdebugread.obj \
	$(INTDIR)/"mem-break.obj" \
	$(INTDIR)/minsyms.obj \
	$(INTDIR)/objfiles.obj \
	$(INTDIR)/parse.obj \
	$(INTDIR)/printcmd.obj \
	$(INTDIR)/"remote-utils.obj" \
	$(INTDIR)/remote.obj \
	$(INTDIR)/serial.obj \
	$(INTDIR)/source.obj \
	$(INTDIR)/stabsread.obj \
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
	$(INTDIR)/"scm-lang.obj" \
	$(INTDIR)/"scm-exp.obj" \
	$(INTDIR)/"scm-valprint.obj" \
	$(INTDIR)/"gnu-regex.obj" \
	$(INTDIR)/dsrec.obj \
	$(INTDIR)/parallel.obj \
	$(INTDIR)/"ch-exp.obj" \
	$(INTDIR)/bcache.obj \
	$(INTDIR)/"coff-m68k.obj" \
	$(INTDIR)/"m68k-dis.obj" \
	$(INTDIR)/"m68k-tdep.obj" \
	$(INTDIR)/"remote-est.obj" \
	$(INTDIR)/"cpu-m68k.obj" \
	$(INTDIR)/aout0.obj \
	$(INTDIR)/aout32.obj \
	$(INTDIR)/"cpu32bug-rom.obj" \
	$(INTDIR)/"rom68k-rom.obj" \
	$(INTDIR)/"m68k-opc.obj" \
	$(INTDIR)/version.obj

"c:\gs\gdb-m68k.exe" : $(OUTDIR)  $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP BASE Use_MFC 5
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "SparcLit"
# PROP BASE Intermediate_Dir "SparcLit"
# PROP Use_MFC 5
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "c:\gs\sparclit"
# PROP Intermediate_Dir "c:\gs\tmp\sparclit"
OUTDIR=c:\gs\sparclit
INTDIR=c:\gs\tmp\sparclit

ALL : "c:\gs\gdb-sp.exe" $(OUTDIR)/gui.bsc

$(OUTDIR) : 
    if not exist $(OUTDIR)/nul mkdir $(OUTDIR)

$(INTDIR) : 
    if not exist $(INTDIR)/nul mkdir $(INTDIR)

MTL_PROJ=
# ADD BASE CPP /nologo /G4 /MT /W3 /GX /Zi /YX /Od /I "g:\gdb\config" /I "g:\gdb\mswin" /I "g:\gdb\mswin\prebuilt\m68k" /I "prebuilt" /I "..\..\mmalloc" /I "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_M68K" /D "_MBCS" /D "TARGET_EST" /FR /c
# ADD CPP /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\config" /I "g:\gdb\mswin" /I "g:\gdb\mswin\prebuilt\sparclite" /I "prebuilt" /I "..\..\mmalloc" /I "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "__WIN32__" /D "_WINDOWS" /D "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_SPARCLITE" /D "_MBCS" /FR /c
# SUBTRACT CPP /WX
CPP_PROJ=/nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\config" /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt\sparclite" /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "__WIN32__" /D\
 "_WINDOWS" /D "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_SPARCLITE" /D "_MBCS"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c 
CPP_OBJS=c:\gs\tmp\sparclit/
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
RSC_PROJ=/l 0x409 /fo$(INTDIR)/"gui.res" /d "_DEBUG" 
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o$(OUTDIR)/"gui.bsc" 
BSC32_SBRS= \
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
	$(INTDIR)/"ser-win32s.sbr" \
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
	$(INTDIR)/"cplus-dem.sbr" \
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
	$(INTDIR)/"dis-buf.sbr" \
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
	$(INTDIR)/stabs.sbr \
	$(INTDIR)/"stab-syms.sbr" \
	$(INTDIR)/annotate.sbr \
	$(INTDIR)/blockframe.sbr \
	$(INTDIR)/breakpoint.sbr \
	$(INTDIR)/buildsym.sbr \
	$(INTDIR)/"c-lang.sbr" \
	$(INTDIR)/"c-typeprint.sbr" \
	$(INTDIR)/"c-valprint.sbr" \
	$(INTDIR)/cexptab.sbr \
	$(INTDIR)/"ch-lang.sbr" \
	$(INTDIR)/"ch-typeprint.sbr" \
	$(INTDIR)/"ch-valprint.sbr" \
	$(INTDIR)/coffread.sbr \
	$(INTDIR)/command.sbr \
	$(INTDIR)/complaints.sbr \
	$(INTDIR)/copying.sbr \
	$(INTDIR)/corefile.sbr \
	$(INTDIR)/"cp-valprint.sbr" \
	$(INTDIR)/dbxread.sbr \
	$(INTDIR)/dcache.sbr \
	$(INTDIR)/demangle.sbr \
	$(INTDIR)/dwarfread.sbr \
	$(INTDIR)/elfread.sbr \
	$(INTDIR)/environ.sbr \
	$(INTDIR)/eval.sbr \
	$(INTDIR)/exec.sbr \
	$(INTDIR)/expprint.sbr \
	$(INTDIR)/"f-lang.sbr" \
	$(INTDIR)/"f-typeprint.sbr" \
	$(INTDIR)/"f-valprint.sbr" \
	$(INTDIR)/fexptab.sbr \
	$(INTDIR)/findvar.sbr \
	$(INTDIR)/gdbtypes.sbr \
	$(INTDIR)/infcmd.sbr \
	$(INTDIR)/infrun.sbr \
	$(INTDIR)/language.sbr \
	$(INTDIR)/"m2-lang.sbr" \
	$(INTDIR)/"m2-typeprint.sbr" \
	$(INTDIR)/"m2-valprint.sbr" \
	$(INTDIR)/m2exptab.sbr \
	$(INTDIR)/main.sbr \
	$(INTDIR)/maint.sbr \
	$(INTDIR)/mdebugread.sbr \
	$(INTDIR)/"mem-break.sbr" \
	$(INTDIR)/minsyms.sbr \
	$(INTDIR)/objfiles.sbr \
	$(INTDIR)/parse.sbr \
	$(INTDIR)/printcmd.sbr \
	$(INTDIR)/"remote-utils.sbr" \
	$(INTDIR)/remote.sbr \
	$(INTDIR)/serial.sbr \
	$(INTDIR)/source.sbr \
	$(INTDIR)/stabsread.sbr \
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
	$(INTDIR)/"scm-lang.sbr" \
	$(INTDIR)/"scm-exp.sbr" \
	$(INTDIR)/"scm-valprint.sbr" \
	$(INTDIR)/"gnu-regex.sbr" \
	$(INTDIR)/dsrec.sbr \
	$(INTDIR)/parallel.sbr \
	$(INTDIR)/"ch-exp.sbr" \
	$(INTDIR)/bcache.sbr \
	$(INTDIR)/"coff-sparc.sbr" \
	$(INTDIR)/"sparc-tdep.sbr" \
	$(INTDIR)/"sparcl-tdep.sbr" \
	$(INTDIR)/"sparc-opc.sbr" \
	$(INTDIR)/"sparc-dis.sbr" \
	$(INTDIR)/"cpu-sparc.sbr" \
	$(INTDIR)/aout32.sbr \
	$(INTDIR)/sunos.sbr \
	$(INTDIR)/version.sbr

$(OUTDIR)/gui.bsc : $(OUTDIR)  $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
# ADD BASE LINK32 /NOLOGO /SUBSYSTEM:windows /DEBUG /MACHINE:I386 /OUT:"c:\gs\gdb-est.exe"
# SUBTRACT BASE LINK32 /VERBOSE /PDB:none
# ADD LINK32 /NOLOGO /SUBSYSTEM:windows /DEBUG /MACHINE:I386 /OUT:"c:\gs\gdb-sp.exe"
# SUBTRACT LINK32 /VERBOSE /PDB:none
LINK32_FLAGS=/NOLOGO /SUBSYSTEM:windows /INCREMENTAL:yes\
 /PDB:$(OUTDIR)/"gui.pdb" /DEBUG /MACHINE:I386 /OUT:"c:\gs\gdb-sp.exe" 
DEF_FILE=
LINK32_OBJS= \
	$(INTDIR)/gui.obj \
	$(INTDIR)/mainfrm.obj \
	$(INTDIR)/stdafx.obj \
	$(INTDIR)/aboutbox.obj \
	$(INTDIR)/iface.obj \
	$(INTDIR)/fontinfo.obj \
	$(INTDIR)/regview.obj \
	$(INTDIR)/regdoc.obj \
	$(INTDIR)/gui.res \
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
	.\serdll32.lib \
	$(INTDIR)/mem.obj \
	$(INTDIR)/bfdcore.obj \
	$(INTDIR)/change.obj \
	$(INTDIR)/frameview.obj \
	$(INTDIR)/log.obj \
	$(INTDIR)/mini.obj \
	$(INTDIR)/option.obj \
	$(INTDIR)/"ser-win32s.obj" \
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
	$(INTDIR)/"cplus-dem.obj" \
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
	$(INTDIR)/"dis-buf.obj" \
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
	$(INTDIR)/stabs.obj \
	$(INTDIR)/"stab-syms.obj" \
	$(INTDIR)/annotate.obj \
	$(INTDIR)/blockframe.obj \
	$(INTDIR)/breakpoint.obj \
	$(INTDIR)/buildsym.obj \
	$(INTDIR)/"c-lang.obj" \
	$(INTDIR)/"c-typeprint.obj" \
	$(INTDIR)/"c-valprint.obj" \
	$(INTDIR)/cexptab.obj \
	$(INTDIR)/"ch-lang.obj" \
	$(INTDIR)/"ch-typeprint.obj" \
	$(INTDIR)/"ch-valprint.obj" \
	$(INTDIR)/coffread.obj \
	$(INTDIR)/command.obj \
	$(INTDIR)/complaints.obj \
	$(INTDIR)/copying.obj \
	$(INTDIR)/corefile.obj \
	$(INTDIR)/"cp-valprint.obj" \
	$(INTDIR)/dbxread.obj \
	$(INTDIR)/dcache.obj \
	$(INTDIR)/demangle.obj \
	$(INTDIR)/dwarfread.obj \
	$(INTDIR)/elfread.obj \
	$(INTDIR)/environ.obj \
	$(INTDIR)/eval.obj \
	$(INTDIR)/exec.obj \
	$(INTDIR)/expprint.obj \
	$(INTDIR)/"f-lang.obj" \
	$(INTDIR)/"f-typeprint.obj" \
	$(INTDIR)/"f-valprint.obj" \
	$(INTDIR)/fexptab.obj \
	$(INTDIR)/findvar.obj \
	$(INTDIR)/gdbtypes.obj \
	$(INTDIR)/infcmd.obj \
	$(INTDIR)/infrun.obj \
	$(INTDIR)/language.obj \
	$(INTDIR)/"m2-lang.obj" \
	$(INTDIR)/"m2-typeprint.obj" \
	$(INTDIR)/"m2-valprint.obj" \
	$(INTDIR)/m2exptab.obj \
	$(INTDIR)/main.obj \
	$(INTDIR)/maint.obj \
	$(INTDIR)/mdebugread.obj \
	$(INTDIR)/"mem-break.obj" \
	$(INTDIR)/minsyms.obj \
	$(INTDIR)/objfiles.obj \
	$(INTDIR)/parse.obj \
	$(INTDIR)/printcmd.obj \
	$(INTDIR)/"remote-utils.obj" \
	$(INTDIR)/remote.obj \
	$(INTDIR)/serial.obj \
	$(INTDIR)/source.obj \
	$(INTDIR)/stabsread.obj \
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
	$(INTDIR)/"scm-lang.obj" \
	$(INTDIR)/"scm-exp.obj" \
	$(INTDIR)/"scm-valprint.obj" \
	$(INTDIR)/"gnu-regex.obj" \
	$(INTDIR)/dsrec.obj \
	$(INTDIR)/parallel.obj \
	$(INTDIR)/"ch-exp.obj" \
	$(INTDIR)/bcache.obj \
	$(INTDIR)/"coff-sparc.obj" \
	$(INTDIR)/"sparc-tdep.obj" \
	$(INTDIR)/"sparcl-tdep.obj" \
	$(INTDIR)/"sparc-opc.obj" \
	$(INTDIR)/"sparc-dis.obj" \
	$(INTDIR)/"cpu-sparc.obj" \
	$(INTDIR)/aout32.obj \
	$(INTDIR)/sunos.obj \
	$(INTDIR)/version.obj

"c:\gs\gdb-sp.exe" : $(OUTDIR)  $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "mips"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "mips_elf"
# PROP BASE Intermediate_Dir "mips_elf"
# PROP Use_MFC 1
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "c:\gs\mips"
# PROP Intermediate_Dir "c:\gs\tmp\mips"
OUTDIR=c:\gs\mips
INTDIR=c:\gs\tmp\mips

ALL : "c:\gs\gdb-mips.exe" $(OUTDIR)/gui.bsc

$(OUTDIR) : 
    if not exist $(OUTDIR)/nul mkdir $(OUTDIR)

$(INTDIR) : 
    if not exist $(INTDIR)/nul mkdir $(INTDIR)

# ADD BASE MTL /nologo /D "NDEBUG" /win32
MTL_PROJ=
# ADD BASE CPP /nologo /W3 /GX /YX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /FR /c
# ADD CPP /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I "g:\gdb\mswin\prebuilt" /I "g:\gdb\mswin\prebuilt\mips" /I "..\..\mmalloc" /I "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I "..\..\readline" /I "..\..\gdb" /I "..\..\gdb\config" /D "_DEBUG" /D "WIN32" /D "__WIN32__" /D "_WINDOWS" /D "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_MIPS" /D "NEED_basename" /D "_MBCS" /FR
CPP_PROJ=/nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt" /I "g:\gdb\mswin\prebuilt\mips" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /I "..\..\gdb\config" /D "_DEBUG" /D "WIN32" /D\
 "__WIN32__" /D "_WINDOWS" /D "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_MIPS"\
 /D "NEED_basename" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" 
CPP_OBJS=c:\gs\tmp\mips/
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
RSC_PROJ=/l 0x409 /fo$(INTDIR)/"gui.res" /d "_DEBUG" 
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o$(OUTDIR)/"gui.bsc" 
BSC32_SBRS= \
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
	$(INTDIR)/"ser-win32s.sbr" \
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
	$(INTDIR)/"cplus-dem.sbr" \
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
	$(INTDIR)/"dis-buf.sbr" \
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
	$(INTDIR)/stabs.sbr \
	$(INTDIR)/"stab-syms.sbr" \
	$(INTDIR)/annotate.sbr \
	$(INTDIR)/blockframe.sbr \
	$(INTDIR)/breakpoint.sbr \
	$(INTDIR)/buildsym.sbr \
	$(INTDIR)/"c-lang.sbr" \
	$(INTDIR)/"c-typeprint.sbr" \
	$(INTDIR)/"c-valprint.sbr" \
	$(INTDIR)/cexptab.sbr \
	$(INTDIR)/"ch-lang.sbr" \
	$(INTDIR)/"ch-typeprint.sbr" \
	$(INTDIR)/"ch-valprint.sbr" \
	$(INTDIR)/coffread.sbr \
	$(INTDIR)/command.sbr \
	$(INTDIR)/complaints.sbr \
	$(INTDIR)/copying.sbr \
	$(INTDIR)/corefile.sbr \
	$(INTDIR)/"cp-valprint.sbr" \
	$(INTDIR)/dbxread.sbr \
	$(INTDIR)/dcache.sbr \
	$(INTDIR)/demangle.sbr \
	$(INTDIR)/dwarfread.sbr \
	$(INTDIR)/elfread.sbr \
	$(INTDIR)/environ.sbr \
	$(INTDIR)/eval.sbr \
	$(INTDIR)/exec.sbr \
	$(INTDIR)/expprint.sbr \
	$(INTDIR)/"f-lang.sbr" \
	$(INTDIR)/"f-typeprint.sbr" \
	$(INTDIR)/"f-valprint.sbr" \
	$(INTDIR)/fexptab.sbr \
	$(INTDIR)/findvar.sbr \
	$(INTDIR)/gdbtypes.sbr \
	$(INTDIR)/infcmd.sbr \
	$(INTDIR)/infrun.sbr \
	$(INTDIR)/language.sbr \
	$(INTDIR)/"m2-lang.sbr" \
	$(INTDIR)/"m2-typeprint.sbr" \
	$(INTDIR)/"m2-valprint.sbr" \
	$(INTDIR)/m2exptab.sbr \
	$(INTDIR)/main.sbr \
	$(INTDIR)/maint.sbr \
	$(INTDIR)/mdebugread.sbr \
	$(INTDIR)/"mem-break.sbr" \
	$(INTDIR)/minsyms.sbr \
	$(INTDIR)/objfiles.sbr \
	$(INTDIR)/parse.sbr \
	$(INTDIR)/printcmd.sbr \
	$(INTDIR)/"remote-utils.sbr" \
	$(INTDIR)/remote.sbr \
	$(INTDIR)/serial.sbr \
	$(INTDIR)/source.sbr \
	$(INTDIR)/stabsread.sbr \
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
	$(INTDIR)/"scm-lang.sbr" \
	$(INTDIR)/"scm-exp.sbr" \
	$(INTDIR)/"scm-valprint.sbr" \
	$(INTDIR)/"gnu-regex.sbr" \
	$(INTDIR)/dsrec.sbr \
	$(INTDIR)/parallel.sbr \
	$(INTDIR)/"ch-exp.sbr" \
	$(INTDIR)/bcache.sbr \
	$(INTDIR)/"remote-mips.sbr" \
	$(INTDIR)/"elf32-mips.sbr" \
	$(INTDIR)/elf.sbr \
	$(INTDIR)/elf32.sbr \
	$(INTDIR)/"cpu-mips.sbr" \
	$(INTDIR)/"mips-tdep.sbr" \
	$(INTDIR)/ecofflink.sbr \
	$(INTDIR)/"coff-mips.sbr" \
	$(INTDIR)/ecoff.sbr \
	$(INTDIR)/"mips-opc.sbr" \
	$(INTDIR)/"mips-dis.sbr" \
	$(INTDIR)/elflink.sbr \
	$(INTDIR)/version.sbr \
	$(INTDIR)/"remote-sim.sbr" \
	$(INTDIR)/interp.sbr

$(OUTDIR)/gui.bsc : $(OUTDIR)  $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /NOLOGO /SUBSYSTEM:windows /MACHINE:I386
# ADD LINK32 /NOLOGO /SUBSYSTEM:windows /INCREMENTAL:yes /DEBUG /MACHINE:I386 /OUT:"c:\gs\gdb-mips.exe"
LINK32_FLAGS=/NOLOGO /SUBSYSTEM:windows /INCREMENTAL:yes\
 /PDB:$(OUTDIR)/"gui.pdb" /DEBUG /MACHINE:I386 /OUT:"c:\gs\gdb-mips.exe" 
DEF_FILE=
LINK32_OBJS= \
	$(INTDIR)/gui.obj \
	$(INTDIR)/mainfrm.obj \
	$(INTDIR)/stdafx.obj \
	$(INTDIR)/aboutbox.obj \
	$(INTDIR)/iface.obj \
	$(INTDIR)/fontinfo.obj \
	$(INTDIR)/regview.obj \
	$(INTDIR)/regdoc.obj \
	$(INTDIR)/gui.res \
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
	.\serdll32.lib \
	$(INTDIR)/mem.obj \
	$(INTDIR)/bfdcore.obj \
	$(INTDIR)/change.obj \
	$(INTDIR)/frameview.obj \
	$(INTDIR)/log.obj \
	$(INTDIR)/mini.obj \
	$(INTDIR)/option.obj \
	$(INTDIR)/"ser-win32s.obj" \
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
	$(INTDIR)/"cplus-dem.obj" \
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
	$(INTDIR)/"dis-buf.obj" \
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
	$(INTDIR)/stabs.obj \
	$(INTDIR)/"stab-syms.obj" \
	$(INTDIR)/annotate.obj \
	$(INTDIR)/blockframe.obj \
	$(INTDIR)/breakpoint.obj \
	$(INTDIR)/buildsym.obj \
	$(INTDIR)/"c-lang.obj" \
	$(INTDIR)/"c-typeprint.obj" \
	$(INTDIR)/"c-valprint.obj" \
	$(INTDIR)/cexptab.obj \
	$(INTDIR)/"ch-lang.obj" \
	$(INTDIR)/"ch-typeprint.obj" \
	$(INTDIR)/"ch-valprint.obj" \
	$(INTDIR)/coffread.obj \
	$(INTDIR)/command.obj \
	$(INTDIR)/complaints.obj \
	$(INTDIR)/copying.obj \
	$(INTDIR)/corefile.obj \
	$(INTDIR)/"cp-valprint.obj" \
	$(INTDIR)/dbxread.obj \
	$(INTDIR)/dcache.obj \
	$(INTDIR)/demangle.obj \
	$(INTDIR)/dwarfread.obj \
	$(INTDIR)/elfread.obj \
	$(INTDIR)/environ.obj \
	$(INTDIR)/eval.obj \
	$(INTDIR)/exec.obj \
	$(INTDIR)/expprint.obj \
	$(INTDIR)/"f-lang.obj" \
	$(INTDIR)/"f-typeprint.obj" \
	$(INTDIR)/"f-valprint.obj" \
	$(INTDIR)/fexptab.obj \
	$(INTDIR)/findvar.obj \
	$(INTDIR)/gdbtypes.obj \
	$(INTDIR)/infcmd.obj \
	$(INTDIR)/infrun.obj \
	$(INTDIR)/language.obj \
	$(INTDIR)/"m2-lang.obj" \
	$(INTDIR)/"m2-typeprint.obj" \
	$(INTDIR)/"m2-valprint.obj" \
	$(INTDIR)/m2exptab.obj \
	$(INTDIR)/main.obj \
	$(INTDIR)/maint.obj \
	$(INTDIR)/mdebugread.obj \
	$(INTDIR)/"mem-break.obj" \
	$(INTDIR)/minsyms.obj \
	$(INTDIR)/objfiles.obj \
	$(INTDIR)/parse.obj \
	$(INTDIR)/printcmd.obj \
	$(INTDIR)/"remote-utils.obj" \
	$(INTDIR)/remote.obj \
	$(INTDIR)/serial.obj \
	$(INTDIR)/source.obj \
	$(INTDIR)/stabsread.obj \
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
	$(INTDIR)/"scm-lang.obj" \
	$(INTDIR)/"scm-exp.obj" \
	$(INTDIR)/"scm-valprint.obj" \
	$(INTDIR)/"gnu-regex.obj" \
	$(INTDIR)/dsrec.obj \
	$(INTDIR)/parallel.obj \
	$(INTDIR)/"ch-exp.obj" \
	$(INTDIR)/bcache.obj \
	$(INTDIR)/"remote-mips.obj" \
	$(INTDIR)/"elf32-mips.obj" \
	$(INTDIR)/elf.obj \
	$(INTDIR)/elf32.obj \
	$(INTDIR)/"cpu-mips.obj" \
	$(INTDIR)/"mips-tdep.obj" \
	$(INTDIR)/ecofflink.obj \
	$(INTDIR)/"coff-mips.obj" \
	$(INTDIR)/ecoff.obj \
	$(INTDIR)/"mips-opc.obj" \
	$(INTDIR)/"mips-dis.obj" \
	$(INTDIR)/elflink.obj \
	$(INTDIR)/version.obj \
	$(INTDIR)/"remote-sim.obj" \
	$(INTDIR)/interp.obj

"c:\gs\gdb-mips.exe" : $(OUTDIR)  $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "a29k"

# PROP BASE Use_MFC 5
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "a29k"
# PROP BASE Intermediate_Dir "a29k"
# PROP Use_MFC 5
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "c:\gs\a29k"
# PROP Intermediate_Dir "c:\gs\tmp\a29k"
OUTDIR=c:\gs\a29k
INTDIR=c:\gs\tmp\a29k

ALL : "c:\gs\gdb-a29k.exe" $(OUTDIR)/gui.bsc

$(OUTDIR) : 
    if not exist $(OUTDIR)/nul mkdir $(OUTDIR)

$(INTDIR) : 
    if not exist $(INTDIR)/nul mkdir $(INTDIR)

MTL_PROJ=
# ADD BASE CPP /nologo /G4 /MT /GX /Zi /YX /Od /I "g:\gdb\mswin" /I "g:\gdb\mswin\prebuilt" /I "g:\gdb\mswin\prebuilt\sh" /I "..\..\mmalloc" /I "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_SH" /D "_MBCS" /c
# ADD CPP /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I "g:\gdb\mswin\prebuilt" /I "g:\gdb\mswin\prebuilt\a29k" /I "..\..\mmalloc" /I "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_A29K" /D "_MBCS" /c
# SUBTRACT CPP /WX
CPP_PROJ=/nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt" /I "g:\gdb\mswin\prebuilt\a29k" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_A29K" /D "_MBCS"\
 /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c 
CPP_OBJS=c:\gs\tmp\a29k/
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
RSC_PROJ=/l 0x409 /fo$(INTDIR)/"gui.res" /d "_DEBUG" 
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o$(OUTDIR)/"gui.bsc" 
BSC32_SBRS= \
	

$(OUTDIR)/gui.bsc : $(OUTDIR)  $(BSC32_SBRS)
LINK32=link.exe
# ADD BASE LINK32 /NOLOGO /SUBSYSTEM:windows /DEBUG /MACHINE:I386 /OUT:"c:\gs\gdb-sh.exe"
# SUBTRACT BASE LINK32 /VERBOSE /PDB:none
# ADD LINK32 /NOLOGO /SUBSYSTEM:windows /DEBUG /MACHINE:I386 /OUT:"c:\gs\gdb-a29k.exe"
# SUBTRACT LINK32 /VERBOSE /PDB:none
LINK32_FLAGS=/NOLOGO /SUBSYSTEM:windows /INCREMENTAL:yes\
 /PDB:$(OUTDIR)/"gui.pdb" /DEBUG /MACHINE:I386 /OUT:"c:\gs\gdb-a29k.exe" 
DEF_FILE=
LINK32_OBJS= \
	$(INTDIR)/gui.obj \
	$(INTDIR)/mainfrm.obj \
	$(INTDIR)/stdafx.obj \
	$(INTDIR)/aboutbox.obj \
	$(INTDIR)/iface.obj \
	$(INTDIR)/fontinfo.obj \
	$(INTDIR)/regview.obj \
	$(INTDIR)/regdoc.obj \
	$(INTDIR)/gui.res \
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
	.\serdll32.lib \
	$(INTDIR)/mem.obj \
	$(INTDIR)/bfdcore.obj \
	$(INTDIR)/change.obj \
	$(INTDIR)/frameview.obj \
	$(INTDIR)/log.obj \
	$(INTDIR)/mini.obj \
	$(INTDIR)/option.obj \
	$(INTDIR)/"ser-win32s.obj" \
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
	$(INTDIR)/"cplus-dem.obj" \
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
	$(INTDIR)/"dis-buf.obj" \
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
	$(INTDIR)/stabs.obj \
	$(INTDIR)/"stab-syms.obj" \
	$(INTDIR)/annotate.obj \
	$(INTDIR)/blockframe.obj \
	$(INTDIR)/breakpoint.obj \
	$(INTDIR)/buildsym.obj \
	$(INTDIR)/"c-lang.obj" \
	$(INTDIR)/"c-typeprint.obj" \
	$(INTDIR)/"c-valprint.obj" \
	$(INTDIR)/cexptab.obj \
	$(INTDIR)/"ch-lang.obj" \
	$(INTDIR)/"ch-typeprint.obj" \
	$(INTDIR)/"ch-valprint.obj" \
	$(INTDIR)/coffread.obj \
	$(INTDIR)/command.obj \
	$(INTDIR)/complaints.obj \
	$(INTDIR)/copying.obj \
	$(INTDIR)/corefile.obj \
	$(INTDIR)/"cp-valprint.obj" \
	$(INTDIR)/dbxread.obj \
	$(INTDIR)/dcache.obj \
	$(INTDIR)/demangle.obj \
	$(INTDIR)/dwarfread.obj \
	$(INTDIR)/elfread.obj \
	$(INTDIR)/environ.obj \
	$(INTDIR)/eval.obj \
	$(INTDIR)/exec.obj \
	$(INTDIR)/expprint.obj \
	$(INTDIR)/"f-lang.obj" \
	$(INTDIR)/"f-typeprint.obj" \
	$(INTDIR)/"f-valprint.obj" \
	$(INTDIR)/fexptab.obj \
	$(INTDIR)/findvar.obj \
	$(INTDIR)/gdbtypes.obj \
	$(INTDIR)/infcmd.obj \
	$(INTDIR)/infrun.obj \
	$(INTDIR)/language.obj \
	$(INTDIR)/"m2-lang.obj" \
	$(INTDIR)/"m2-typeprint.obj" \
	$(INTDIR)/"m2-valprint.obj" \
	$(INTDIR)/m2exptab.obj \
	$(INTDIR)/main.obj \
	$(INTDIR)/maint.obj \
	$(INTDIR)/mdebugread.obj \
	$(INTDIR)/"mem-break.obj" \
	$(INTDIR)/minsyms.obj \
	$(INTDIR)/objfiles.obj \
	$(INTDIR)/parse.obj \
	$(INTDIR)/printcmd.obj \
	$(INTDIR)/"remote-utils.obj" \
	$(INTDIR)/remote.obj \
	$(INTDIR)/serial.obj \
	$(INTDIR)/source.obj \
	$(INTDIR)/stabsread.obj \
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
	$(INTDIR)/"scm-lang.obj" \
	$(INTDIR)/"scm-exp.obj" \
	$(INTDIR)/"scm-valprint.obj" \
	$(INTDIR)/"gnu-regex.obj" \
	$(INTDIR)/dsrec.obj \
	$(INTDIR)/parallel.obj \
	$(INTDIR)/"ch-exp.obj" \
	$(INTDIR)/bcache.obj \
	$(INTDIR)/"a29k-tdep.obj" \
	$(INTDIR)/"coff-a29k.obj" \
	$(INTDIR)/"cpu-a29k.obj" \
	$(INTDIR)/udi2go32.obj \
	$(INTDIR)/"remote-udi.obj" \
	$(INTDIR)/udr.obj \
	$(INTDIR)/udip2soc.obj \
	$(INTDIR)/"a29k-dis.obj" \
	$(INTDIR)/version.obj

"c:\gs\gdb-a29k.exe" : $(OUTDIR)  $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "i386"

# PROP BASE Use_MFC 5
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "i386"
# PROP BASE Intermediate_Dir "i386"
# PROP Use_MFC 5
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "c:\gs\i386"
# PROP Intermediate_Dir "c:\gs\tmp\i386"
OUTDIR=c:\gs\i386
INTDIR=c:\gs\tmp\i386

ALL : "c:\gs\gdb-i386.exe" $(OUTDIR)/gui.bsc

$(OUTDIR) : 
    if not exist $(OUTDIR)/nul mkdir $(OUTDIR)

$(INTDIR) : 
    if not exist $(INTDIR)/nul mkdir $(INTDIR)

MTL_PROJ=
# ADD BASE CPP /nologo /G4 /MT /GX /Zi /YX /Od /I "g:\gdb\mswin" /I "g:\gdb\mswin\prebuilt" /I "g:\gdb\mswin\prebuilt\sh" /I "..\..\mmalloc" /I "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_SH" /D "_MBCS" /c
# ADD CPP /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I "g:\gdb\mswin\prebuilt" /I "g:\gdb\mswin\prebuilt\i386" /I "..\..\mmalloc" /I "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_I386" /D "_MBCS" /c
# SUBTRACT CPP /WX
CPP_PROJ=/nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt" /I "g:\gdb\mswin\prebuilt\i386" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_I386" /D "_MBCS"\
 /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c 
CPP_OBJS=c:\gs\tmp\i386/
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
RSC_PROJ=/l 0x409 /fo$(INTDIR)/"gui.res" /d "_DEBUG" 
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o$(OUTDIR)/"gui.bsc" 
BSC32_SBRS= \
	

$(OUTDIR)/gui.bsc : $(OUTDIR)  $(BSC32_SBRS)
LINK32=link.exe
# ADD BASE LINK32 /NOLOGO /SUBSYSTEM:windows /DEBUG /MACHINE:I386 /OUT:"c:\gs\gdb-sh.exe"
# SUBTRACT BASE LINK32 /VERBOSE /PDB:none
# ADD LINK32 /NOLOGO /SUBSYSTEM:windows /DEBUG /MACHINE:I386 /OUT:"c:\gs\gdb-i386.exe"
# SUBTRACT LINK32 /VERBOSE /PDB:none
LINK32_FLAGS=/NOLOGO /SUBSYSTEM:windows /INCREMENTAL:yes\
 /PDB:$(OUTDIR)/"gui.pdb" /DEBUG /MACHINE:I386 /OUT:"c:\gs\gdb-i386.exe" 
DEF_FILE=
LINK32_OBJS= \
	$(INTDIR)/gui.obj \
	$(INTDIR)/mainfrm.obj \
	$(INTDIR)/stdafx.obj \
	$(INTDIR)/aboutbox.obj \
	$(INTDIR)/iface.obj \
	$(INTDIR)/fontinfo.obj \
	$(INTDIR)/regview.obj \
	$(INTDIR)/regdoc.obj \
	$(INTDIR)/gui.res \
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
	.\serdll32.lib \
	$(INTDIR)/mem.obj \
	$(INTDIR)/bfdcore.obj \
	$(INTDIR)/change.obj \
	$(INTDIR)/frameview.obj \
	$(INTDIR)/log.obj \
	$(INTDIR)/mini.obj \
	$(INTDIR)/option.obj \
	$(INTDIR)/"ser-win32s.obj" \
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
	$(INTDIR)/"cplus-dem.obj" \
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
	$(INTDIR)/"dis-buf.obj" \
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
	$(INTDIR)/stabs.obj \
	$(INTDIR)/"stab-syms.obj" \
	$(INTDIR)/annotate.obj \
	$(INTDIR)/blockframe.obj \
	$(INTDIR)/breakpoint.obj \
	$(INTDIR)/buildsym.obj \
	$(INTDIR)/"c-lang.obj" \
	$(INTDIR)/"c-typeprint.obj" \
	$(INTDIR)/"c-valprint.obj" \
	$(INTDIR)/cexptab.obj \
	$(INTDIR)/"ch-lang.obj" \
	$(INTDIR)/"ch-typeprint.obj" \
	$(INTDIR)/"ch-valprint.obj" \
	$(INTDIR)/coffread.obj \
	$(INTDIR)/command.obj \
	$(INTDIR)/complaints.obj \
	$(INTDIR)/copying.obj \
	$(INTDIR)/corefile.obj \
	$(INTDIR)/"cp-valprint.obj" \
	$(INTDIR)/dbxread.obj \
	$(INTDIR)/dcache.obj \
	$(INTDIR)/demangle.obj \
	$(INTDIR)/dwarfread.obj \
	$(INTDIR)/elfread.obj \
	$(INTDIR)/environ.obj \
	$(INTDIR)/eval.obj \
	$(INTDIR)/exec.obj \
	$(INTDIR)/expprint.obj \
	$(INTDIR)/"f-lang.obj" \
	$(INTDIR)/"f-typeprint.obj" \
	$(INTDIR)/"f-valprint.obj" \
	$(INTDIR)/fexptab.obj \
	$(INTDIR)/findvar.obj \
	$(INTDIR)/gdbtypes.obj \
	$(INTDIR)/infcmd.obj \
	$(INTDIR)/infrun.obj \
	$(INTDIR)/language.obj \
	$(INTDIR)/"m2-lang.obj" \
	$(INTDIR)/"m2-typeprint.obj" \
	$(INTDIR)/"m2-valprint.obj" \
	$(INTDIR)/m2exptab.obj \
	$(INTDIR)/main.obj \
	$(INTDIR)/maint.obj \
	$(INTDIR)/mdebugread.obj \
	$(INTDIR)/"mem-break.obj" \
	$(INTDIR)/minsyms.obj \
	$(INTDIR)/objfiles.obj \
	$(INTDIR)/parse.obj \
	$(INTDIR)/printcmd.obj \
	$(INTDIR)/"remote-utils.obj" \
	$(INTDIR)/remote.obj \
	$(INTDIR)/serial.obj \
	$(INTDIR)/source.obj \
	$(INTDIR)/stabsread.obj \
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
	$(INTDIR)/"scm-lang.obj" \
	$(INTDIR)/"scm-exp.obj" \
	$(INTDIR)/"scm-valprint.obj" \
	$(INTDIR)/"gnu-regex.obj" \
	$(INTDIR)/dsrec.obj \
	$(INTDIR)/parallel.obj \
	$(INTDIR)/"ch-exp.obj" \
	$(INTDIR)/bcache.obj \
	$(INTDIR)/"cpu-i386.obj" \
	$(INTDIR)/"i386-tdep.obj" \
	$(INTDIR)/"i386-dis.obj" \
	$(INTDIR)/aout32.obj \
	$(INTDIR)/i386aout.obj \
	$(INTDIR)/version.obj

"c:\gs\gdb-i386.exe" : $(OUTDIR)  $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 

.c{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

.cpp{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

.cxx{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

################################################################################
# Begin Group "gui"

################################################################################
# Begin Source File

SOURCE=.\gui.cpp
DEP_GUI_C=\
	.\stdafx.h\
	.\aboutbox.h\
	.\log.h\
	.\fsplit.h\
	.\mainfrm.h\
	.\regview.h\
	.\regdoc.h\
	.\expwin.h\
	.\gdbdoc.h\
	.\browserl.h\
	.\srcb.h\
	.\bpt.h\
	.\framevie.h\
	.\bptdoc.h\
	.\srcsel.h\
	.\srcd.h\
	.\srcwin.h\
	.\option.h\
	.\ginfodoc.h\
	.\infofram.h\
	.\mem.h\
	.\mini.h\
	.\gui.h

!IF  "$(CFG)" == "sh"

$(INTDIR)/gui.obj :  $(SOURCE)  $(DEP_GUI_C) $(INTDIR)

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/gui.obj :  $(SOURCE)  $(DEP_GUI_C) $(INTDIR)

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/gui.obj :  $(SOURCE)  $(DEP_GUI_C) $(INTDIR)

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/gui.obj :  $(SOURCE)  $(DEP_GUI_C) $(INTDIR)

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/gui.obj :  $(SOURCE)  $(DEP_GUI_C) $(INTDIR)

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/gui.obj :  $(SOURCE)  $(DEP_GUI_C) $(INTDIR)

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/gui.obj :  $(SOURCE)  $(DEP_GUI_C) $(INTDIR)

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/gui.obj :  $(SOURCE)  $(DEP_GUI_C) $(INTDIR)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\mainfrm.cpp

!IF  "$(CFG)" == "sh"

$(INTDIR)/mainfrm.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/mainfrm.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/mainfrm.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/mainfrm.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/mainfrm.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/mainfrm.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/mainfrm.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/mainfrm.obj :  $(SOURCE)  $(INTDIR)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\readme.txt
# End Source File
################################################################################
# Begin Source File

SOURCE=.\stdafx.cpp
DEP_STDAF=\
	.\stdafx.h\
	.\mini.h\
	.\gui.h

!IF  "$(CFG)" == "sh"

$(INTDIR)/stdafx.obj :  $(SOURCE)  $(DEP_STDAF) $(INTDIR)

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/stdafx.obj :  $(SOURCE)  $(DEP_STDAF) $(INTDIR)

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/stdafx.obj :  $(SOURCE)  $(DEP_STDAF) $(INTDIR)

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/stdafx.obj :  $(SOURCE)  $(DEP_STDAF) $(INTDIR)

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/stdafx.obj :  $(SOURCE)  $(DEP_STDAF) $(INTDIR)

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/stdafx.obj :  $(SOURCE)  $(DEP_STDAF) $(INTDIR)

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/stdafx.obj :  $(SOURCE)  $(DEP_STDAF) $(INTDIR)

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/stdafx.obj :  $(SOURCE)  $(DEP_STDAF) $(INTDIR)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\aboutbox.cpp
DEP_ABOUT=\
	.\stdafx.h\
	.\mini.h\
	.\gui.h

!IF  "$(CFG)" == "sh"

$(INTDIR)/aboutbox.obj :  $(SOURCE)  $(DEP_ABOUT) $(INTDIR)

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/aboutbox.obj :  $(SOURCE)  $(DEP_ABOUT) $(INTDIR)

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/aboutbox.obj :  $(SOURCE)  $(DEP_ABOUT) $(INTDIR)

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/aboutbox.obj :  $(SOURCE)  $(DEP_ABOUT) $(INTDIR)

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/aboutbox.obj :  $(SOURCE)  $(DEP_ABOUT) $(INTDIR)

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/aboutbox.obj :  $(SOURCE)  $(DEP_ABOUT) $(INTDIR)

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/aboutbox.obj :  $(SOURCE)  $(DEP_ABOUT) $(INTDIR)

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/aboutbox.obj :  $(SOURCE)  $(DEP_ABOUT) $(INTDIR)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\iface.cpp
DEP_IFACE=\
	.\stdafx.h\
	.\regdoc.h\
	.\bptdoc.h\
	.\thinking.h\
	.\log.h\
	.\mini.h\
	.\gui.h

!IF  "$(CFG)" == "sh"

$(INTDIR)/iface.obj :  $(SOURCE)  $(DEP_IFACE) $(INTDIR)

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/iface.obj :  $(SOURCE)  $(DEP_IFACE) $(INTDIR)

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/iface.obj :  $(SOURCE)  $(DEP_IFACE) $(INTDIR)

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/iface.obj :  $(SOURCE)  $(DEP_IFACE) $(INTDIR)

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/iface.obj :  $(SOURCE)  $(DEP_IFACE) $(INTDIR)

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/iface.obj :  $(SOURCE)  $(DEP_IFACE) $(INTDIR)

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/iface.obj :  $(SOURCE)  $(DEP_IFACE) $(INTDIR)

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/iface.obj :  $(SOURCE)  $(DEP_IFACE) $(INTDIR)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\fontinfo.cpp
DEP_FONTI=\
	.\stdafx.h\
	.\mini.h\
	.\gui.h

!IF  "$(CFG)" == "sh"

$(INTDIR)/fontinfo.obj :  $(SOURCE)  $(DEP_FONTI) $(INTDIR)

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/fontinfo.obj :  $(SOURCE)  $(DEP_FONTI) $(INTDIR)

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/fontinfo.obj :  $(SOURCE)  $(DEP_FONTI) $(INTDIR)

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/fontinfo.obj :  $(SOURCE)  $(DEP_FONTI) $(INTDIR)

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/fontinfo.obj :  $(SOURCE)  $(DEP_FONTI) $(INTDIR)

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/fontinfo.obj :  $(SOURCE)  $(DEP_FONTI) $(INTDIR)

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/fontinfo.obj :  $(SOURCE)  $(DEP_FONTI) $(INTDIR)

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/fontinfo.obj :  $(SOURCE)  $(DEP_FONTI) $(INTDIR)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\regview.cpp

!IF  "$(CFG)" == "sh"

$(INTDIR)/regview.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/regview.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/regview.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/regview.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/regview.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/regview.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/regview.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/regview.obj :  $(SOURCE)  $(INTDIR)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\regdoc.cpp
DEP_REGDO=\
	.\stdafx.h\
	.\regdoc.h\
	.\mini.h\
	.\gui.h

!IF  "$(CFG)" == "sh"

$(INTDIR)/regdoc.obj :  $(SOURCE)  $(DEP_REGDO) $(INTDIR)

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/regdoc.obj :  $(SOURCE)  $(DEP_REGDO) $(INTDIR)

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/regdoc.obj :  $(SOURCE)  $(DEP_REGDO) $(INTDIR)

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/regdoc.obj :  $(SOURCE)  $(DEP_REGDO) $(INTDIR)

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/regdoc.obj :  $(SOURCE)  $(DEP_REGDO) $(INTDIR)

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/regdoc.obj :  $(SOURCE)  $(DEP_REGDO) $(INTDIR)

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/regdoc.obj :  $(SOURCE)  $(DEP_REGDO) $(INTDIR)

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/regdoc.obj :  $(SOURCE)  $(DEP_REGDO) $(INTDIR)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\gui.rc
DEP_GUI_R=\
	.\res\guidoc.ico\
	.\res\idr_cmdf.ico\
	.\res\ico00001.ico\
	.\res\idr_srct.ico\
	.\res\idr_srcb.ico\
	.\res\ico00004.ico\
	.\res\idr_regt.ico\
	.\res\idr_main.ico\
	.\res\infodoc.ico\
	.\res\N_gdb.ico\
	.\res\icon1.ico\
	.\res\memdoc.ico\
	.\res\id_sym_i.ico\
	.\res\icon2.ico\
	.\res\toolbar.bmp\
	.\res\bitmap1.bmp\
	.\res\idr_main.bmp\
	.\res\idr_wint.bmp\
	.\res\bmp00007.bmp\
	.\res\bmp00001.bmp\
	.\res\bmp00002.bmp\
	.\res\bmp00003.bmp\
	.\res\cdown.bmp\
	.\res\cf.bmp\
	.\res\bmp00004.bmp\
	.\res\cursor1.cur\
	.\res\cur00001.cur\
	.\res\bpt_curs.cur\
	.\res\gui.rc2\
	.\menus\menus.rc

!IF  "$(CFG)" == "sh"

$(INTDIR)/gui.res :  $(SOURCE)  $(DEP_GUI_R) $(INTDIR)
   $(RSC) $(RSC_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/gui.res :  $(SOURCE)  $(DEP_GUI_R) $(INTDIR)
   $(RSC) $(RSC_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/gui.res :  $(SOURCE)  $(DEP_GUI_R) $(INTDIR)
   $(RSC) $(RSC_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/gui.res :  $(SOURCE)  $(DEP_GUI_R) $(INTDIR)
   $(RSC) $(RSC_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/gui.res :  $(SOURCE)  $(DEP_GUI_R) $(INTDIR)
   $(RSC) $(RSC_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/gui.res :  $(SOURCE)  $(DEP_GUI_R) $(INTDIR)
   $(RSC) $(RSC_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/gui.res :  $(SOURCE)  $(DEP_GUI_R) $(INTDIR)
   $(RSC) $(RSC_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/gui.res :  $(SOURCE)  $(DEP_GUI_R) $(INTDIR)
   $(RSC) $(RSC_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\bptdoc.cpp

!IF  "$(CFG)" == "sh"

$(INTDIR)/bptdoc.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/bptdoc.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/bptdoc.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/bptdoc.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/bptdoc.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/bptdoc.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/bptdoc.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/bptdoc.obj :  $(SOURCE)  $(INTDIR)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\colinfo.cpp
DEP_COLIN=\
	.\stdafx.h\
	.\colinfo.h\
	.\mini.h\
	.\gui.h

!IF  "$(CFG)" == "sh"

$(INTDIR)/colinfo.obj :  $(SOURCE)  $(DEP_COLIN) $(INTDIR)

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/colinfo.obj :  $(SOURCE)  $(DEP_COLIN) $(INTDIR)

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/colinfo.obj :  $(SOURCE)  $(DEP_COLIN) $(INTDIR)

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/colinfo.obj :  $(SOURCE)  $(DEP_COLIN) $(INTDIR)

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/colinfo.obj :  $(SOURCE)  $(DEP_COLIN) $(INTDIR)

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/colinfo.obj :  $(SOURCE)  $(DEP_COLIN) $(INTDIR)

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/colinfo.obj :  $(SOURCE)  $(DEP_COLIN) $(INTDIR)

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/colinfo.obj :  $(SOURCE)  $(DEP_COLIN) $(INTDIR)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\gdbwrap.cpp
DEP_GDBWR=\
	.\prebuilt\sh\tm.h\
	..\breakpoint.h\
	..\symtab.h\
	.\gdbtypes.h\
	.\gdbwrap.h

!IF  "$(CFG)" == "sh"

$(INTDIR)/gdbwrap.obj :  $(SOURCE)  $(DEP_GDBWR) $(INTDIR)

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/gdbwrap.obj :  $(SOURCE)  $(DEP_GDBWR) $(INTDIR)

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/gdbwrap.obj :  $(SOURCE)  $(DEP_GDBWR) $(INTDIR)

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/gdbwrap.obj :  $(SOURCE)  $(DEP_GDBWR) $(INTDIR)

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/gdbwrap.obj :  $(SOURCE)  $(DEP_GDBWR) $(INTDIR)

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/gdbwrap.obj :  $(SOURCE)  $(DEP_GDBWR) $(INTDIR)

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/gdbwrap.obj :  $(SOURCE)  $(DEP_GDBWR) $(INTDIR)

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/gdbwrap.obj :  $(SOURCE)  $(DEP_GDBWR) $(INTDIR)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\srcbrows.cpp

!IF  "$(CFG)" == "sh"

$(INTDIR)/srcbrows.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/srcbrows.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/srcbrows.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/srcbrows.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/srcbrows.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/srcbrows.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/srcbrows.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/srcbrows.obj :  $(SOURCE)  $(INTDIR)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\scview.cpp

!IF  "$(CFG)" == "sh"

$(INTDIR)/scview.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/scview.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/scview.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/scview.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/scview.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/scview.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/scview.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/scview.obj :  $(SOURCE)  $(INTDIR)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\framevie.cpp

!IF  "$(CFG)" == "sh"

$(INTDIR)/framevie.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/framevie.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/framevie.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/framevie.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/framevie.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/framevie.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/framevie.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/framevie.obj :  $(SOURCE)  $(INTDIR)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\browserl.cpp

!IF  "$(CFG)" == "sh"

$(INTDIR)/browserl.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/browserl.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/browserl.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/browserl.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/browserl.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/browserl.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/browserl.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/browserl.obj :  $(SOURCE)  $(INTDIR)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\browserf.cpp
DEP_BROWS=\
	.\stdafx.h\
	.\browserf.h\
	.\mini.h\
	.\gui.h

!IF  "$(CFG)" == "sh"

$(INTDIR)/browserf.obj :  $(SOURCE)  $(DEP_BROWS) $(INTDIR)

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/browserf.obj :  $(SOURCE)  $(DEP_BROWS) $(INTDIR)

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/browserf.obj :  $(SOURCE)  $(DEP_BROWS) $(INTDIR)

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/browserf.obj :  $(SOURCE)  $(DEP_BROWS) $(INTDIR)

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/browserf.obj :  $(SOURCE)  $(DEP_BROWS) $(INTDIR)

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/browserf.obj :  $(SOURCE)  $(DEP_BROWS) $(INTDIR)

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/browserf.obj :  $(SOURCE)  $(DEP_BROWS) $(INTDIR)

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/browserf.obj :  $(SOURCE)  $(DEP_BROWS) $(INTDIR)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\gdbdoc.cpp
DEP_GDBDO=\
	.\stdafx.h\
	.\gdbdoc.h\
	.\mini.h\
	.\gui.h

!IF  "$(CFG)" == "sh"

$(INTDIR)/gdbdoc.obj :  $(SOURCE)  $(DEP_GDBDO) $(INTDIR)

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/gdbdoc.obj :  $(SOURCE)  $(DEP_GDBDO) $(INTDIR)

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/gdbdoc.obj :  $(SOURCE)  $(DEP_GDBDO) $(INTDIR)

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/gdbdoc.obj :  $(SOURCE)  $(DEP_GDBDO) $(INTDIR)

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/gdbdoc.obj :  $(SOURCE)  $(DEP_GDBDO) $(INTDIR)

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/gdbdoc.obj :  $(SOURCE)  $(DEP_GDBDO) $(INTDIR)

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/gdbdoc.obj :  $(SOURCE)  $(DEP_GDBDO) $(INTDIR)

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/gdbdoc.obj :  $(SOURCE)  $(DEP_GDBDO) $(INTDIR)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\flash.cpp

!IF  "$(CFG)" == "sh"

$(INTDIR)/flash.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/flash.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/flash.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/flash.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/flash.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/flash.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/flash.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/flash.obj :  $(SOURCE)  $(INTDIR)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\stubs.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/stubs.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/stubs.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/stubs.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/stubs.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/stubs.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/stubs.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/stubs.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/stubs.obj :  $(SOURCE)  $(INTDIR)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\gdbwin.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/gdbwin.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/gdbwin.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/gdbwin.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/gdbwin.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/gdbwin.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/gdbwin.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/gdbwin.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/gdbwin.obj :  $(SOURCE)  $(INTDIR)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\gdbwinxx.cpp

!IF  "$(CFG)" == "sh"

$(INTDIR)/gdbwinxx.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/gdbwinxx.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/gdbwinxx.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/gdbwinxx.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/gdbwinxx.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/gdbwinxx.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/gdbwinxx.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/gdbwinxx.obj :  $(SOURCE)  $(INTDIR)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\initfake.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/initfake.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/initfake.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/initfake.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/initfake.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/initfake.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/initfake.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/initfake.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/initfake.obj :  $(SOURCE)  $(INTDIR)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\infoframe.cpp
DEP_INFOF=\
	.\stdafx.h\
	.\ginfodoc.h\
	.\infofram.h\
	.\mini.h\
	.\gui.h

!IF  "$(CFG)" == "sh"

$(INTDIR)/infoframe.obj :  $(SOURCE)  $(DEP_INFOF) $(INTDIR)

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/infoframe.obj :  $(SOURCE)  $(DEP_INFOF) $(INTDIR)

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/infoframe.obj :  $(SOURCE)  $(DEP_INFOF) $(INTDIR)

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/infoframe.obj :  $(SOURCE)  $(DEP_INFOF) $(INTDIR)

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/infoframe.obj :  $(SOURCE)  $(DEP_INFOF) $(INTDIR)

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/infoframe.obj :  $(SOURCE)  $(DEP_INFOF) $(INTDIR)

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/infoframe.obj :  $(SOURCE)  $(DEP_INFOF) $(INTDIR)

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/infoframe.obj :  $(SOURCE)  $(DEP_INFOF) $(INTDIR)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\ginfodoc.cpp

!IF  "$(CFG)" == "sh"

$(INTDIR)/ginfodoc.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/ginfodoc.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/ginfodoc.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/ginfodoc.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/ginfodoc.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/ginfodoc.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/ginfodoc.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/ginfodoc.obj :  $(SOURCE)  $(INTDIR)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\srcwin.cpp

!IF  "$(CFG)" == "sh"

$(INTDIR)/srcwin.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/srcwin.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/srcwin.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/srcwin.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/srcwin.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/srcwin.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/srcwin.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/srcwin.obj :  $(SOURCE)  $(INTDIR)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\srcd.cpp

!IF  "$(CFG)" == "sh"

$(INTDIR)/srcd.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/srcd.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/srcd.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/srcd.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/srcd.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/srcd.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/srcd.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/srcd.obj :  $(SOURCE)  $(INTDIR)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\transbmp.cpp
DEP_TRANS=\
	.\stdafx.h\
	.\transbmp.h\
	.\mini.h\
	.\gui.h

!IF  "$(CFG)" == "sh"

$(INTDIR)/transbmp.obj :  $(SOURCE)  $(DEP_TRANS) $(INTDIR)

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/transbmp.obj :  $(SOURCE)  $(DEP_TRANS) $(INTDIR)

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/transbmp.obj :  $(SOURCE)  $(DEP_TRANS) $(INTDIR)

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/transbmp.obj :  $(SOURCE)  $(DEP_TRANS) $(INTDIR)

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/transbmp.obj :  $(SOURCE)  $(DEP_TRANS) $(INTDIR)

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/transbmp.obj :  $(SOURCE)  $(DEP_TRANS) $(INTDIR)

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/transbmp.obj :  $(SOURCE)  $(DEP_TRANS) $(INTDIR)

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/transbmp.obj :  $(SOURCE)  $(DEP_TRANS) $(INTDIR)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\expwin.cpp

!IF  "$(CFG)" == "sh"

$(INTDIR)/expwin.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/expwin.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/expwin.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/expwin.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/expwin.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/expwin.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/expwin.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/expwin.obj :  $(SOURCE)  $(INTDIR)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\fsplit.cpp

!IF  "$(CFG)" == "sh"

$(INTDIR)/fsplit.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/fsplit.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/fsplit.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/fsplit.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/fsplit.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/fsplit.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/fsplit.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/fsplit.obj :  $(SOURCE)  $(INTDIR)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\srcsel.cpp
DEP_SRCSE=\
	.\stdafx.h\
	.\srcsel.h\
	.\srcd.h\
	.\srcwin.h\
	.\mini.h\
	.\gui.h

!IF  "$(CFG)" == "sh"

$(INTDIR)/srcsel.obj :  $(SOURCE)  $(DEP_SRCSE) $(INTDIR)

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/srcsel.obj :  $(SOURCE)  $(DEP_SRCSE) $(INTDIR)

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/srcsel.obj :  $(SOURCE)  $(DEP_SRCSE) $(INTDIR)

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/srcsel.obj :  $(SOURCE)  $(DEP_SRCSE) $(INTDIR)

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/srcsel.obj :  $(SOURCE)  $(DEP_SRCSE) $(INTDIR)

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/srcsel.obj :  $(SOURCE)  $(DEP_SRCSE) $(INTDIR)

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/srcsel.obj :  $(SOURCE)  $(DEP_SRCSE) $(INTDIR)

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/srcsel.obj :  $(SOURCE)  $(DEP_SRCSE) $(INTDIR)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\props.cpp

!IF  "$(CFG)" == "sh"

$(INTDIR)/props.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/props.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/props.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/props.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/props.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/props.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/props.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/props.obj :  $(SOURCE)  $(INTDIR)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\dirpkr.cpp
DEP_DIRPK=\
	.\stdafx.h\
	.\mini.h\
	.\gui.h

!IF  "$(CFG)" == "sh"

$(INTDIR)/dirpkr.obj :  $(SOURCE)  $(DEP_DIRPK) $(INTDIR)

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/dirpkr.obj :  $(SOURCE)  $(DEP_DIRPK) $(INTDIR)

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/dirpkr.obj :  $(SOURCE)  $(DEP_DIRPK) $(INTDIR)

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/dirpkr.obj :  $(SOURCE)  $(DEP_DIRPK) $(INTDIR)

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/dirpkr.obj :  $(SOURCE)  $(DEP_DIRPK) $(INTDIR)

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/dirpkr.obj :  $(SOURCE)  $(DEP_DIRPK) $(INTDIR)

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/dirpkr.obj :  $(SOURCE)  $(DEP_DIRPK) $(INTDIR)

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/dirpkr.obj :  $(SOURCE)  $(DEP_DIRPK) $(INTDIR)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\srcb.cpp

!IF  "$(CFG)" == "sh"

$(INTDIR)/srcb.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/srcb.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/srcb.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/srcb.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/srcb.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/srcb.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/srcb.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/srcb.obj :  $(SOURCE)  $(INTDIR)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\serdll32.lib
# End Source File
################################################################################
# Begin Source File

SOURCE=.\mem.cpp
DEP_MEM_C=\
	.\stdafx.h\
	.\regdoc.h\
	.\mem.h\
	.\mini.h\
	.\gui.h

!IF  "$(CFG)" == "sh"

$(INTDIR)/mem.obj :  $(SOURCE)  $(DEP_MEM_C) $(INTDIR)

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/mem.obj :  $(SOURCE)  $(DEP_MEM_C) $(INTDIR)

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/mem.obj :  $(SOURCE)  $(DEP_MEM_C) $(INTDIR)

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/mem.obj :  $(SOURCE)  $(DEP_MEM_C) $(INTDIR)

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/mem.obj :  $(SOURCE)  $(DEP_MEM_C) $(INTDIR)

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/mem.obj :  $(SOURCE)  $(DEP_MEM_C) $(INTDIR)

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/mem.obj :  $(SOURCE)  $(DEP_MEM_C) $(INTDIR)

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/mem.obj :  $(SOURCE)  $(DEP_MEM_C) $(INTDIR)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\bfdcore.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/bfdcore.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/bfdcore.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/bfdcore.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/bfdcore.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/bfdcore.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/bfdcore.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/bfdcore.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/bfdcore.obj :  $(SOURCE)  $(INTDIR)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\change.cpp

!IF  "$(CFG)" == "sh"

$(INTDIR)/change.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/change.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/change.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/change.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/change.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/change.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/change.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/change.obj :  $(SOURCE)  $(INTDIR)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\frameview.cpp

!IF  "$(CFG)" == "sh"

$(INTDIR)/frameview.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/frameview.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/frameview.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/frameview.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/frameview.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/frameview.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/frameview.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/frameview.obj :  $(SOURCE)  $(INTDIR)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\log.cpp

!IF  "$(CFG)" == "sh"

$(INTDIR)/log.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/log.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/log.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/log.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/log.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/log.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/log.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/log.obj :  $(SOURCE)  $(INTDIR)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\mini.cpp
DEP_MINI_=\
	.\stdafx.h\
	.\mini.h\
	.\gui.h

!IF  "$(CFG)" == "sh"

$(INTDIR)/mini.obj :  $(SOURCE)  $(DEP_MINI_) $(INTDIR)

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/mini.obj :  $(SOURCE)  $(DEP_MINI_) $(INTDIR)

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/mini.obj :  $(SOURCE)  $(DEP_MINI_) $(INTDIR)

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/mini.obj :  $(SOURCE)  $(DEP_MINI_) $(INTDIR)

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/mini.obj :  $(SOURCE)  $(DEP_MINI_) $(INTDIR)

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/mini.obj :  $(SOURCE)  $(DEP_MINI_) $(INTDIR)

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/mini.obj :  $(SOURCE)  $(DEP_MINI_) $(INTDIR)

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/mini.obj :  $(SOURCE)  $(DEP_MINI_) $(INTDIR)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\option.cpp

!IF  "$(CFG)" == "sh"

$(INTDIR)/option.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/option.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/option.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/option.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/option.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/option.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/option.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/option.obj :  $(SOURCE)  $(INTDIR)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=".\ser-win32s.c"

!IF  "$(CFG)" == "sh"

$(INTDIR)/"ser-win32s.obj" :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"ser-win32s.obj" :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/"ser-win32s.obj" :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/"ser-win32s.obj" :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/"ser-win32s.obj" :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/"ser-win32s.obj" :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/"ser-win32s.obj" :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/"ser-win32s.obj" :  $(SOURCE)  $(INTDIR)

!ENDIF 

# End Source File
# End Group
################################################################################
# Begin Group "libiberty"

################################################################################
# Begin Source File

SOURCE=..\..\libiberty\alloca.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/alloca.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/alloca.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/alloca.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/alloca.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/alloca.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/alloca.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/alloca.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/alloca.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\..\libiberty\argv.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/argv.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/argv.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/argv.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/argv.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/argv.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/argv.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/argv.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/argv.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\..\libiberty\bcmp.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/bcmp.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/bcmp.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/bcmp.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/bcmp.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/bcmp.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/bcmp.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/bcmp.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/bcmp.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\..\libiberty\bzero.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/bzero.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/bzero.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/bzero.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/bzero.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/bzero.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/bzero.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/bzero.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/bzero.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\..\libiberty\obstack.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/obstack.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/obstack.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/obstack.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/obstack.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/obstack.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/obstack.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/obstack.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/obstack.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\..\libiberty\random.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/random.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/random.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/random.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/random.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/random.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/random.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/random.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/random.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\..\libiberty\rindex.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/rindex.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/rindex.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/rindex.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/rindex.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/rindex.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/rindex.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/rindex.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/rindex.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\..\libiberty\spaces.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/spaces.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/spaces.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/spaces.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/spaces.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/spaces.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/spaces.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/spaces.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/spaces.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\..\libiberty\bcopy.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/bcopy.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/bcopy.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/bcopy.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/bcopy.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/bcopy.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/bcopy.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/bcopy.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/bcopy.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\..\libiberty\concat.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/concat.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/concat.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/concat.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/concat.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/concat.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/concat.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/concat.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/concat.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\..\libiberty\strtod.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/strtod.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/strtod.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/strtod.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/strtod.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/strtod.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/strtod.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/strtod.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/strtod.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\libiberty\cplus-dem.c"

!IF  "$(CFG)" == "sh"

$(INTDIR)/"cplus-dem.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"cplus-dem.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/"cplus-dem.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/"cplus-dem.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/"cplus-dem.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/"cplus-dem.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/"cplus-dem.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/"cplus-dem.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\..\libiberty\vprintf.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/vprintf.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/vprintf.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/vprintf.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/vprintf.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/vprintf.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/vprintf.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/vprintf.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/vprintf.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\..\libiberty\tmpnam.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/tmpnam.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/tmpnam.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/tmpnam.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/tmpnam.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/tmpnam.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/tmpnam.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/tmpnam.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/tmpnam.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\..\libiberty\vasprintf.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/vasprintf.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/vasprintf.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/vasprintf.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/vasprintf.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/vasprintf.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/vasprintf.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/vasprintf.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/vasprintf.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\..\libiberty\strdup.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/strdup.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/strdup.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/strdup.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/strdup.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/strdup.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/strdup.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/strdup.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/strdup.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\..\libiberty\getopt1.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/getopt1.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/getopt1.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/getopt1.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/getopt1.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/getopt1.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/getopt1.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/getopt1.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/getopt1.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\..\libiberty\insque.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/insque.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/insque.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/insque.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/insque.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/insque.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/insque.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/insque.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/insque.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\..\libiberty\getopt.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/getopt.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/getopt.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/getopt.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/getopt.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/getopt.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/getopt.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/getopt.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/getopt.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\libiberty\hex.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/hex.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/hex.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/hex.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/hex.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/hex.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/hex.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/hex.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/hex.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\libiberty\getruntime.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/getruntime.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/getruntime.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/getruntime.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/getruntime.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/getruntime.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/getruntime.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/getruntime.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/getruntime.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\libiberty\floatformat.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/floatformat.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/floatformat.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/floatformat.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/floatformat.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/floatformat.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/floatformat.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/floatformat.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/floatformat.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\libiberty\strcasecmp.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/strcasecmp.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/strcasecmp.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/strcasecmp.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/strcasecmp.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/strcasecmp.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/strcasecmp.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/strcasecmp.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/strcasecmp.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\libiberty\basename.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/basename.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/basename.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/basename.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/basename.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/basename.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/basename.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/basename.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/basename.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
# End Group
################################################################################
# Begin Group "opcodes"

################################################################################
# Begin Source File

SOURCE="\opcodes\dis-buf.c"

!IF  "$(CFG)" == "sh"

$(INTDIR)/"dis-buf.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"dis-buf.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/"dis-buf.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/"dis-buf.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/"dis-buf.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/"dis-buf.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/"dis-buf.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/"dis-buf.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\..\opcodes\disassemble.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/disassemble.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/disassemble.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/disassemble.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/disassemble.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/disassemble.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/disassemble.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/disassemble.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/disassemble.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
# End Group
################################################################################
# Begin Group "readline"

################################################################################
# Begin Source File

SOURCE=..\..\readline\readline.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/readline.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/readline.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/readline.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/readline.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/readline.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/readline.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/readline.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/readline.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\..\readline\search.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/search.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/search.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/search.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/search.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/search.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/search.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/search.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/search.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\..\readline\signals.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/signals.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/signals.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/signals.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/signals.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/signals.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/signals.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/signals.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/signals.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\..\readline\keymaps.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/keymaps.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/keymaps.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/keymaps.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/keymaps.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/keymaps.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/keymaps.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/keymaps.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/keymaps.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\..\readline\funmap.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/funmap.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/funmap.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/funmap.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/funmap.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/funmap.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/funmap.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/funmap.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/funmap.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\..\readline\isearch.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/isearch.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/isearch.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/isearch.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/isearch.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/isearch.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/isearch.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/isearch.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/isearch.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\..\readline\display.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/display.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/display.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/display.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/display.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/display.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/display.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/display.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/display.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\..\readline\parens.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/parens.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/parens.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/parens.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/parens.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/parens.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/parens.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/parens.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/parens.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\..\readline\bind.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/bind.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/bind.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/bind.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/bind.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/bind.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/bind.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/bind.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/bind.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\..\readline\rltty.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/rltty.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/rltty.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/rltty.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/rltty.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/rltty.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/rltty.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/rltty.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/rltty.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\..\readline\complete.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/complete.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/complete.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/complete.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/complete.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/complete.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/complete.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/complete.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/complete.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\..\readline\history.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/history.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/history.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/history.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/history.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/history.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/history.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/history.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/history.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
# End Group
################################################################################
# Begin Group "common-bfd"

################################################################################
# Begin Source File

SOURCE=..\..\bfd\archive.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/archive.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/archive.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/archive.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/archive.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/archive.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/archive.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/archive.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/archive.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\..\bfd\archures.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/archures.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/archures.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/archures.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/archures.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/archures.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/archures.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/archures.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/archures.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\..\bfd\bfd.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/bfd.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/bfd.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/bfd.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/bfd.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/bfd.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/bfd.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/bfd.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/bfd.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\bfd\binary.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/binary.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/binary.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/binary.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/binary.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/binary.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/binary.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/binary.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/binary.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\..\bfd\cache.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/cache.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/cache.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/cache.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/cache.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/cache.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/cache.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/cache.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/cache.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\..\bfd\coffgen.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/coffgen.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/coffgen.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/coffgen.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/coffgen.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/coffgen.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/coffgen.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/coffgen.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/coffgen.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\..\bfd\cofflink.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/cofflink.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/cofflink.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/cofflink.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/cofflink.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/cofflink.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/cofflink.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/cofflink.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/cofflink.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\..\bfd\filemode.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/filemode.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/filemode.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/filemode.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/filemode.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/filemode.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/filemode.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/filemode.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/filemode.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\..\bfd\format.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/format.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/format.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/format.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/format.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/format.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/format.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/format.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/format.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\..\bfd\hash.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/hash.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/hash.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/hash.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/hash.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/hash.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/hash.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/hash.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/hash.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\..\bfd\init.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/init.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/init.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/init.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/init.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/init.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/init.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/init.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/init.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\..\bfd\libbfd.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/libbfd.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/libbfd.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/libbfd.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/libbfd.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/libbfd.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/libbfd.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/libbfd.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/libbfd.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\..\bfd\linker.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/linker.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/linker.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/linker.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/linker.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/linker.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/linker.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/linker.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/linker.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\..\bfd\opncls.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/opncls.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/opncls.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/opncls.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/opncls.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/opncls.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/opncls.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/opncls.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/opncls.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\..\bfd\reloc.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/reloc.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/reloc.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/reloc.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/reloc.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/reloc.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/reloc.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/reloc.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/reloc.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\..\bfd\section.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/section.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/section.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/section.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/section.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/section.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/section.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/section.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/section.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\..\bfd\srec.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/srec.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/srec.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/srec.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/srec.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/srec.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/srec.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/srec.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/srec.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\..\bfd\syms.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/syms.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/syms.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/syms.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/syms.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/syms.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/syms.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/syms.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/syms.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\..\bfd\targets.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/targets.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/targets.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/targets.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/targets.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/targets.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/targets.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/targets.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/targets.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\bpt.cpp

!IF  "$(CFG)" == "sh"

$(INTDIR)/bpt.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/bpt.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/bpt.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/bpt.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/bpt.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/bpt.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/bpt.obj :  $(SOURCE)  $(INTDIR)

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/bpt.obj :  $(SOURCE)  $(INTDIR)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\bfd\tekhex.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/tekhex.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/tekhex.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/tekhex.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/tekhex.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/tekhex.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/tekhex.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/tekhex.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/tekhex.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\bfd\versados.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/versados.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/versados.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/versados.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/versados.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/versados.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/versados.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/versados.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/versados.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\bfd\ihex.c
DEP_IHEX_=\
	.\prebuilt\bfd.h\
	\bfd\sysdep.h\
	\bfd\libbfd.h\
	\include\libiberty.h

!IF  "$(CFG)" == "sh"

$(INTDIR)/ihex.obj :  $(SOURCE)  $(DEP_IHEX_) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/ihex.obj :  $(SOURCE)  $(DEP_IHEX_) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/ihex.obj :  $(SOURCE)  $(DEP_IHEX_) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/ihex.obj :  $(SOURCE)  $(DEP_IHEX_) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/ihex.obj :  $(SOURCE)  $(DEP_IHEX_) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/ihex.obj :  $(SOURCE)  $(DEP_IHEX_) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/ihex.obj :  $(SOURCE)  $(DEP_IHEX_) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/ihex.obj :  $(SOURCE)  $(DEP_IHEX_) $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\bfd\stabs.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/stabs.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/stabs.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/stabs.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/stabs.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/stabs.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/stabs.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/stabs.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/stabs.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\bfd\stab-syms.c"

!IF  "$(CFG)" == "sh"

$(INTDIR)/"stab-syms.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"stab-syms.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/"stab-syms.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/"stab-syms.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/"stab-syms.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/"stab-syms.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/"stab-syms.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/"stab-syms.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
# End Group
################################################################################
# Begin Group "common-gdb"

################################################################################
# Begin Source File

SOURCE=..\annotate.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/annotate.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/annotate.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/annotate.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/annotate.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/annotate.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/annotate.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/annotate.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/annotate.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\blockframe.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/blockframe.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/blockframe.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/blockframe.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/blockframe.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/blockframe.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/blockframe.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/blockframe.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/blockframe.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\breakpoint.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/breakpoint.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/breakpoint.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/breakpoint.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/breakpoint.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/breakpoint.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/breakpoint.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/breakpoint.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/breakpoint.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\buildsym.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/buildsym.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/buildsym.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/buildsym.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/buildsym.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/buildsym.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/buildsym.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/buildsym.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/buildsym.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\gdb\c-lang.c"

!IF  "$(CFG)" == "sh"

$(INTDIR)/"c-lang.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"c-lang.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/"c-lang.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/"c-lang.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/"c-lang.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/"c-lang.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/"c-lang.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/"c-lang.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\gdb\c-typeprint.c"

!IF  "$(CFG)" == "sh"

$(INTDIR)/"c-typeprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"c-typeprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/"c-typeprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/"c-typeprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/"c-typeprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/"c-typeprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/"c-typeprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/"c-typeprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\gdb\c-valprint.c"

!IF  "$(CFG)" == "sh"

$(INTDIR)/"c-valprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"c-valprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/"c-valprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/"c-valprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/"c-valprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/"c-valprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/"c-valprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/"c-valprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\prebuilt\gdb\cexptab.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/cexptab.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/cexptab.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/cexptab.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/cexptab.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/cexptab.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/cexptab.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/cexptab.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/cexptab.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\gdb\ch-lang.c"

!IF  "$(CFG)" == "sh"

$(INTDIR)/"ch-lang.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"ch-lang.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/"ch-lang.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/"ch-lang.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/"ch-lang.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/"ch-lang.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/"ch-lang.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/"ch-lang.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\gdb\ch-typeprint.c"

!IF  "$(CFG)" == "sh"

$(INTDIR)/"ch-typeprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"ch-typeprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/"ch-typeprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/"ch-typeprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/"ch-typeprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/"ch-typeprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/"ch-typeprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/"ch-typeprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\gdb\ch-valprint.c"

!IF  "$(CFG)" == "sh"

$(INTDIR)/"ch-valprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"ch-valprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/"ch-valprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/"ch-valprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/"ch-valprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/"ch-valprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/"ch-valprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/"ch-valprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\coffread.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/coffread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/coffread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/coffread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/coffread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/coffread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/coffread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/coffread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/coffread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\command.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/command.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/command.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/command.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/command.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/command.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/command.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/command.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/command.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\complaints.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/complaints.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/complaints.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/complaints.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/complaints.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/complaints.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/complaints.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/complaints.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/complaints.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\prebuilt\gdb\copying.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/copying.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/copying.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/copying.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/copying.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/copying.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/copying.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/copying.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/copying.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\corefile.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/corefile.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/corefile.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/corefile.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/corefile.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/corefile.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/corefile.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/corefile.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/corefile.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\gdb\cp-valprint.c"

!IF  "$(CFG)" == "sh"

$(INTDIR)/"cp-valprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"cp-valprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/"cp-valprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/"cp-valprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/"cp-valprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/"cp-valprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/"cp-valprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/"cp-valprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\gdb\dbxread.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/dbxread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/dbxread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/dbxread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/dbxread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/dbxread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/dbxread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/dbxread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/dbxread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\dcache.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/dcache.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/dcache.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/dcache.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/dcache.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/dcache.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/dcache.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/dcache.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/dcache.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\demangle.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/demangle.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/demangle.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/demangle.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/demangle.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/demangle.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/demangle.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/demangle.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/demangle.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\gdb\dwarfread.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/dwarfread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/dwarfread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/dwarfread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/dwarfread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/dwarfread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/dwarfread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/dwarfread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/dwarfread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\gdb\elfread.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/elfread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/elfread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/elfread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/elfread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/elfread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/elfread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/elfread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/elfread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\environ.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/environ.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/environ.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/environ.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/environ.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/environ.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/environ.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/environ.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/environ.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\eval.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/eval.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/eval.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/eval.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/eval.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/eval.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/eval.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/eval.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/eval.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\exec.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/exec.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/exec.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/exec.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/exec.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/exec.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/exec.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/exec.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/exec.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\expprint.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/expprint.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/expprint.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/expprint.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/expprint.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/expprint.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/expprint.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/expprint.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/expprint.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\gdb\f-exp.tab.c"
# End Source File
################################################################################
# Begin Source File

SOURCE="\gdb\f-lang.c"

!IF  "$(CFG)" == "sh"

$(INTDIR)/"f-lang.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"f-lang.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/"f-lang.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/"f-lang.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/"f-lang.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/"f-lang.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/"f-lang.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/"f-lang.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\gdb\f-typeprint.c"

!IF  "$(CFG)" == "sh"

$(INTDIR)/"f-typeprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"f-typeprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/"f-typeprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/"f-typeprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/"f-typeprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/"f-typeprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/"f-typeprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/"f-typeprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\gdb\f-valprint.c"

!IF  "$(CFG)" == "sh"

$(INTDIR)/"f-valprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"f-valprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/"f-valprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/"f-valprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/"f-valprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/"f-valprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/"f-valprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/"f-valprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\prebuilt\gdb\fexptab.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/fexptab.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/fexptab.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/fexptab.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/fexptab.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/fexptab.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/fexptab.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/fexptab.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/fexptab.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\findvar.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/findvar.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/findvar.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/findvar.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/findvar.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/findvar.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/findvar.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/findvar.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/findvar.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\gdbtypes.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/gdbtypes.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/gdbtypes.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/gdbtypes.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/gdbtypes.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/gdbtypes.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/gdbtypes.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/gdbtypes.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/gdbtypes.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\infcmd.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/infcmd.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/infcmd.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/infcmd.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/infcmd.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/infcmd.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/infcmd.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/infcmd.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/infcmd.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\infrun.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/infrun.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/infrun.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/infrun.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/infrun.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/infrun.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/infrun.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/infrun.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/infrun.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\language.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/language.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/language.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/language.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/language.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/language.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/language.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/language.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/language.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\gdb\m2-lang.c"

!IF  "$(CFG)" == "sh"

$(INTDIR)/"m2-lang.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"m2-lang.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/"m2-lang.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/"m2-lang.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/"m2-lang.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/"m2-lang.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/"m2-lang.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/"m2-lang.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\gdb\m2-typeprint.c"

!IF  "$(CFG)" == "sh"

$(INTDIR)/"m2-typeprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"m2-typeprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/"m2-typeprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/"m2-typeprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/"m2-typeprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/"m2-typeprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/"m2-typeprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/"m2-typeprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\gdb\m2-valprint.c"

!IF  "$(CFG)" == "sh"

$(INTDIR)/"m2-valprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"m2-valprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/"m2-valprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/"m2-valprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/"m2-valprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/"m2-valprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/"m2-valprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/"m2-valprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\prebuilt\gdb\m2exptab.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/m2exptab.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/m2exptab.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/m2exptab.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/m2exptab.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/m2exptab.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/m2exptab.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/m2exptab.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/m2exptab.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\main.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/main.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/main.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/main.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/main.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/main.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/main.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/main.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/main.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\maint.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/maint.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/maint.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/maint.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/maint.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/maint.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/maint.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/maint.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/maint.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\gdb\mdebugread.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/mdebugread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/mdebugread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/mdebugread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/mdebugread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/mdebugread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/mdebugread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/mdebugread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/mdebugread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\gdb\mem-break.c"

!IF  "$(CFG)" == "sh"

$(INTDIR)/"mem-break.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"mem-break.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/"mem-break.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/"mem-break.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/"mem-break.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/"mem-break.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/"mem-break.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/"mem-break.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\minsyms.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/minsyms.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/minsyms.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/minsyms.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/minsyms.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/minsyms.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/minsyms.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/minsyms.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/minsyms.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\objfiles.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/objfiles.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/objfiles.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/objfiles.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/objfiles.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/objfiles.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/objfiles.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/objfiles.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/objfiles.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\parse.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/parse.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/parse.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/parse.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/parse.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/parse.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/parse.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/parse.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/parse.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\printcmd.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/printcmd.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/printcmd.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/printcmd.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/printcmd.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/printcmd.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/printcmd.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/printcmd.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/printcmd.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\gdb\remote-utils.c"

!IF  "$(CFG)" == "sh"

$(INTDIR)/"remote-utils.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"remote-utils.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/"remote-utils.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/"remote-utils.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/"remote-utils.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/"remote-utils.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/"remote-utils.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/"remote-utils.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\remote.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/remote.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/remote.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/remote.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/remote.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/remote.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/remote.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/remote.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/remote.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\serial.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/serial.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/serial.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/serial.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/serial.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/serial.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/serial.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/serial.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/serial.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\source.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/source.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/source.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/source.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/source.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/source.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/source.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/source.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/source.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\stabsread.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/stabsread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/stabsread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/stabsread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/stabsread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/stabsread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/stabsread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/stabsread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/stabsread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\stack.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/stack.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/stack.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/stack.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/stack.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/stack.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/stack.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/stack.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/stack.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\symfile.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/symfile.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/symfile.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/symfile.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/symfile.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/symfile.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/symfile.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/symfile.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/symfile.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\symmisc.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/symmisc.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/symmisc.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/symmisc.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/symmisc.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/symmisc.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/symmisc.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/symmisc.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/symmisc.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\symtab.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/symtab.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/symtab.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/symtab.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/symtab.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/symtab.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/symtab.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/symtab.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/symtab.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\target.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/target.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/target.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/target.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/target.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/target.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/target.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/target.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/target.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\thread.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/thread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/thread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/thread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/thread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/thread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/thread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/thread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/thread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\top.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/top.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/top.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/top.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/top.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/top.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/top.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/top.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/top.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\typeprint.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/typeprint.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/typeprint.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/typeprint.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/typeprint.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/typeprint.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/typeprint.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/typeprint.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/typeprint.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\utils.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/utils.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/utils.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/utils.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/utils.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/utils.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/utils.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/utils.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/utils.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\valarith.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/valarith.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/valarith.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/valarith.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/valarith.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/valarith.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/valarith.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/valarith.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/valarith.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\valops.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/valops.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/valops.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/valops.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/valops.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/valops.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/valops.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/valops.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/valops.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\valprint.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/valprint.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/valprint.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/valprint.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/valprint.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/valprint.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/valprint.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/valprint.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/valprint.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\values.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/values.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/values.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/values.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/values.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/values.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/values.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/values.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/values.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\gdb\monitor.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/monitor.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/monitor.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/monitor.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/monitor.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/monitor.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/monitor.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/monitor.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/monitor.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\gdb\nlmread.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/nlmread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/nlmread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/nlmread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/nlmread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/nlmread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/nlmread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/nlmread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/nlmread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\gdb\os9kread.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/os9kread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/os9kread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/os9kread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/os9kread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/os9kread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/os9kread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/os9kread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/os9kread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\gdb\mipsread.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/mipsread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/mipsread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/mipsread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/mipsread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/mipsread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/mipsread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/mipsread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/mipsread.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\gdb\callback.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/callback.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/callback.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/callback.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/callback.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/callback.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/callback.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/callback.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/callback.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\gdb\scm-lang.c"

!IF  "$(CFG)" == "sh"

$(INTDIR)/"scm-lang.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"scm-lang.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/"scm-lang.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/"scm-lang.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/"scm-lang.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/"scm-lang.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/"scm-lang.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/"scm-lang.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\gdb\scm-exp.c"

!IF  "$(CFG)" == "sh"

$(INTDIR)/"scm-exp.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"scm-exp.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/"scm-exp.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/"scm-exp.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/"scm-exp.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/"scm-exp.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/"scm-exp.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/"scm-exp.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\gdb\scm-valprint.c"

!IF  "$(CFG)" == "sh"

$(INTDIR)/"scm-valprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"scm-valprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/"scm-valprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/"scm-valprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/"scm-valprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/"scm-valprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/"scm-valprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/"scm-valprint.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\gdb\gnu-regex.c"

!IF  "$(CFG)" == "sh"

$(INTDIR)/"gnu-regex.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"gnu-regex.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/"gnu-regex.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/"gnu-regex.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/"gnu-regex.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/"gnu-regex.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/"gnu-regex.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/"gnu-regex.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\gdb\dsrec.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/dsrec.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/dsrec.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/dsrec.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/dsrec.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/dsrec.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/dsrec.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/dsrec.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/dsrec.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\parallel.cpp
DEP_PARAL=\
	\gdb\defs.h

!IF  "$(CFG)" == "sh"

$(INTDIR)/parallel.obj :  $(SOURCE)  $(DEP_PARAL) $(INTDIR)

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/parallel.obj :  $(SOURCE)  $(DEP_PARAL) $(INTDIR)

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/parallel.obj :  $(SOURCE)  $(DEP_PARAL) $(INTDIR)

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/parallel.obj :  $(SOURCE)  $(DEP_PARAL) $(INTDIR)

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/parallel.obj :  $(SOURCE)  $(DEP_PARAL) $(INTDIR)

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/parallel.obj :  $(SOURCE)  $(DEP_PARAL) $(INTDIR)

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/parallel.obj :  $(SOURCE)  $(DEP_PARAL) $(INTDIR)

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/parallel.obj :  $(SOURCE)  $(DEP_PARAL) $(INTDIR)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\gdb\ch-exp.c"

!IF  "$(CFG)" == "sh"

$(INTDIR)/"ch-exp.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"ch-exp.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/"ch-exp.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/"ch-exp.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/"ch-exp.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/"ch-exp.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/"ch-exp.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/"ch-exp.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\gdb\bcache.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/bcache.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/bcache.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/bcache.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

$(INTDIR)/bcache.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/bcache.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/bcache.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/bcache.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/bcache.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) $(CPP_PROJ)  $(SOURCE) 

!ENDIF 

# End Source File
# End Group
################################################################################
# Begin Group "sh"

################################################################################
# Begin Source File

SOURCE="\gdb\sh-tdep.c"

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 0

$(INTDIR)/"sh-tdep.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /MT /W3 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt\sh" /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_SH" /D "_MBCS" /D "NEED_basename"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c\
  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"sh-tdep.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Intermediate_Dir "c:\gs\tmp\h8300"
# PROP Exclude_From_Build 1
INTDIR_SRC=c:\gs\tmp\h8300

!ELSEIF  "$(CFG)" == "m68k"

# PROP BASE Exclude_From_Build 0
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "a29k"

# PROP BASE Exclude_From_Build 0
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP BASE Exclude_From_Build 0
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=..\..\sim\sh\interp.c

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 0

$(INTDIR)/interp.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /MT /W3 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt\sh" /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_SH" /D "_MBCS" /D "NEED_basename"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c\
  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/interp.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Intermediate_Dir "c:\gs\tmp\h8300"
# PROP Exclude_From_Build 1
INTDIR_SRC=c:\gs\tmp\h8300

!ELSEIF  "$(CFG)" == "m68k"

# PROP BASE Exclude_From_Build 0
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "a29k"

# PROP BASE Exclude_From_Build 0
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP BASE Exclude_From_Build 0
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\prebuilt\sh\table.c

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 0

$(INTDIR)/table.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /MT /W3 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt\sh" /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_SH" /D "_MBCS" /D "NEED_basename"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c\
  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/table.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Intermediate_Dir "c:\gs\tmp\h8300"
# PROP Exclude_From_Build 1
INTDIR_SRC=c:\gs\tmp\h8300

!ELSEIF  "$(CFG)" == "m68k"

# PROP BASE Exclude_From_Build 0
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "a29k"

# PROP BASE Exclude_From_Build 0
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP BASE Exclude_From_Build 0
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\opcodes\sh-dis.c"

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 0

$(INTDIR)/"sh-dis.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /MT /W3 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt\sh" /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_SH" /D "_MBCS" /D "NEED_basename"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c\
  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"sh-dis.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Intermediate_Dir "c:\gs\tmp\h8300"
# PROP Exclude_From_Build 1
INTDIR_SRC=c:\gs\tmp\h8300

!ELSEIF  "$(CFG)" == "m68k"

# PROP BASE Exclude_From_Build 0
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "a29k"

# PROP BASE Exclude_From_Build 0
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP BASE Exclude_From_Build 0
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\bfd\cpu-sh.c"

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 0

$(INTDIR)/"cpu-sh.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /MT /W3 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt\sh" /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_SH" /D "_MBCS" /D "NEED_basename"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c\
  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"cpu-sh.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Intermediate_Dir "c:\gs\tmp\h8300"
# PROP Exclude_From_Build 1
INTDIR_SRC=c:\gs\tmp\h8300

!ELSEIF  "$(CFG)" == "m68k"

# PROP BASE Exclude_From_Build 0
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "a29k"

# PROP BASE Exclude_From_Build 0
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP BASE Exclude_From_Build 0
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\bfd\coff-sh.c"

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 0

$(INTDIR)/"coff-sh.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /MT /W3 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt\sh" /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_SH" /D "_MBCS" /D "NEED_basename"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c\
  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"coff-sh.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Intermediate_Dir "c:\gs\tmp\h8300"
# PROP Exclude_From_Build 1
INTDIR_SRC=c:\gs\tmp\h8300

!ELSEIF  "$(CFG)" == "m68k"

# PROP BASE Exclude_From_Build 0
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "a29k"

# PROP BASE Exclude_From_Build 0
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP BASE Exclude_From_Build 0
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\gdb\remote-sim.c"

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 0

$(INTDIR)/"remote-sim.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /MT /W3 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt\sh" /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_SH" /D "_MBCS" /D "NEED_basename"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c\
  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"remote-sim.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "a29k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\prebuilt\sh\version.c

!IF  "$(CFG)" == "sh"

$(INTDIR)/version.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /MT /W3 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt\sh" /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_SH" /D "_MBCS" /D "NEED_basename"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c\
  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/version.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "a29k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\gdb\sh3-rom.c"

!IF  "$(CFG)" == "sh"

$(INTDIR)/"sh3-rom.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /MT /W3 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt\sh" /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_SH" /D "_MBCS" /D "NEED_basename"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c\
  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"sh3-rom.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "a29k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\gdb\remote-e7000.c"

!IF  "$(CFG)" == "sh"

$(INTDIR)/"remote-e7000.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /MT /W3 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt\sh" /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_SH" /D "_MBCS" /D "NEED_basename"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c\
  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"remote-e7000.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "a29k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\gdb\ser-e7kpc.c"

!IF  "$(CFG)" == "sh"

$(INTDIR)/"ser-e7kpc.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /MT /W3 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt\sh" /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_SH" /D "_MBCS" /D "NEED_basename"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c\
  $(SOURCE) 

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"ser-e7kpc.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "a29k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# End Group
################################################################################
# Begin Group "h8300"

################################################################################
# Begin Source File

SOURCE="\bfd\cpu-h8300.c"

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"cpu-h8300.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 0

$(INTDIR)/"cpu-h8300.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt" /I "g:\gdb\mswin\prebuilt\h8300" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_H8300" /D "_MBCS" /D "NEED_basename"\
 /Fr$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c\
  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "a29k"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\sim\h8300\compile.c

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/compile.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 0

$(INTDIR)/compile.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt" /I "g:\gdb\mswin\prebuilt\h8300" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_H8300" /D "_MBCS" /D "NEED_basename"\
 /Fr$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c\
  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "a29k"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\bfd\coff-h8300.c"

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"coff-h8300.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 0

$(INTDIR)/"coff-h8300.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt" /I "g:\gdb\mswin\prebuilt\h8300" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_H8300" /D "_MBCS" /D "NEED_basename"\
 /Fr$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c\
  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "a29k"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\gdb\h8300-tdep.c"

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"h8300-tdep.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 0

$(INTDIR)/"h8300-tdep.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt" /I "g:\gdb\mswin\prebuilt\h8300" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_H8300" /D "_MBCS" /D "NEED_basename"\
 /Fr$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c\
  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "a29k"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\bfd\reloc16.c

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/reloc16.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 0

$(INTDIR)/reloc16.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt" /I "g:\gdb\mswin\prebuilt\h8300" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_H8300" /D "_MBCS" /D "NEED_basename"\
 /Fr$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c\
  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "a29k"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\opcodes\h8300-dis.c"

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"h8300-dis.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 0

$(INTDIR)/"h8300-dis.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt" /I "g:\gdb\mswin\prebuilt\h8300" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_H8300" /D "_MBCS" /D "NEED_basename"\
 /Fr$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c\
  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "a29k"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\gdb\remote-hms.c"

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"remote-hms.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 0

$(INTDIR)/"remote-hms.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt" /I "g:\gdb\mswin\prebuilt\h8300" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_H8300" /D "_MBCS" /D "NEED_basename"\
 /Fr$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c\
  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "a29k"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\gdb\remote-sim.c"

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"remote-sim.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 0

$(INTDIR)/"remote-sim.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt" /I "g:\gdb\mswin\prebuilt\h8300" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_H8300" /D "_MBCS" /D "NEED_basename"\
 /Fr$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c\
  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "a29k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\prebuilt\h8300\version.c

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/version.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/version.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt" /I "g:\gdb\mswin\prebuilt\h8300" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_H8300" /D "_MBCS" /D "NEED_basename"\
 /Fr$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c\
  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "a29k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\gdb\remote-e7000.c"

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"remote-e7000.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/"remote-e7000.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt" /I "g:\gdb\mswin\prebuilt\h8300" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_H8300" /D "_MBCS" /D "NEED_basename"\
 /Fr$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c\
  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "a29k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\gdb\ser-e7kpc.c"

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"ser-e7kpc.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

$(INTDIR)/"ser-e7kpc.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt" /I "g:\gdb\mswin\prebuilt\h8300" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_H8300" /D "_MBCS" /D "NEED_basename"\
 /Fr$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c\
  $(SOURCE) 

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "a29k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# End Group
################################################################################
# Begin Group "m68k"

################################################################################
# Begin Source File

SOURCE="\bfd\coff-m68k.c"

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"coff-m68k.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 0

$(INTDIR)/"coff-m68k.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\config" /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt\m68k" /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "__WIN32__" /D\
 "_WINDOWS" /D "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_M68K" /D "_MBCS" /D\
 "TARGET_EST" /Fr$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/\
 /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "a29k"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\opcodes\m68k-dis.c"

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"m68k-dis.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 0

$(INTDIR)/"m68k-dis.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\config" /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt\m68k" /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "__WIN32__" /D\
 "_WINDOWS" /D "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_M68K" /D "_MBCS" /D\
 "TARGET_EST" /Fr$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/\
 /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "a29k"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\gdb\m68k-tdep.c"

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"m68k-tdep.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 0

$(INTDIR)/"m68k-tdep.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\config" /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt\m68k" /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "__WIN32__" /D\
 "_WINDOWS" /D "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_M68K" /D "_MBCS" /D\
 "TARGET_EST" /Fr$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/\
 /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "a29k"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\gdb\remote-est.c"

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"remote-est.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 0

$(INTDIR)/"remote-est.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\config" /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt\m68k" /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "__WIN32__" /D\
 "_WINDOWS" /D "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_M68K" /D "_MBCS" /D\
 "TARGET_EST" /Fr$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/\
 /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "a29k"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\bfd\cpu-m68k.c"

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"cpu-m68k.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 0

$(INTDIR)/"cpu-m68k.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\config" /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt\m68k" /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "__WIN32__" /D\
 "_WINDOWS" /D "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_M68K" /D "_MBCS" /D\
 "TARGET_EST" /Fr$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/\
 /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "a29k"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\bfd\aout0.c

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/aout0.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 0

$(INTDIR)/aout0.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\config" /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt\m68k" /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "__WIN32__" /D\
 "_WINDOWS" /D "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_M68K" /D "_MBCS" /D\
 "TARGET_EST" /Fr$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/\
 /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "a29k"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\bfd\aout32.c

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/aout32.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 0

$(INTDIR)/aout32.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\config" /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt\m68k" /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "__WIN32__" /D\
 "_WINDOWS" /D "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_M68K" /D "_MBCS" /D\
 "TARGET_EST" /Fr$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/\
 /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "a29k"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\bfd\stab-syms.c"

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"stab-syms.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 0

$(INTDIR)/"stab-syms.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\config" /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt\m68k" /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "__WIN32__" /D\
 "_WINDOWS" /D "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_M68K" /D "_MBCS" /D\
 "TARGET_EST" /Fr$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/\
 /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "a29k"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\gdb\cpu32bug-rom.c"

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"cpu32bug-rom.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 0

$(INTDIR)/"cpu32bug-rom.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\config" /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt\m68k" /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "__WIN32__" /D\
 "_WINDOWS" /D "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_M68K" /D "_MBCS" /D\
 "TARGET_EST" /Fr$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/\
 /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "a29k"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\gdb\rom68k-rom.c"

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"rom68k-rom.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 0

$(INTDIR)/"rom68k-rom.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\config" /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt\m68k" /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "__WIN32__" /D\
 "_WINDOWS" /D "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_M68K" /D "_MBCS" /D\
 "TARGET_EST" /Fr$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/\
 /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "a29k"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\opcodes\m68k-opc.c"

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"m68k-opc.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 0

$(INTDIR)/"m68k-opc.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\config" /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt\m68k" /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "__WIN32__" /D\
 "_WINDOWS" /D "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_M68K" /D "_MBCS" /D\
 "TARGET_EST" /Fr$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/\
 /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "a29k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\prebuilt\m68k\version.c

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/version.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 0

$(INTDIR)/version.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\config" /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt\m68k" /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "__WIN32__" /D\
 "_WINDOWS" /D "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_M68K" /D "_MBCS" /D\
 "TARGET_EST" /Fr$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/\
 /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "a29k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# End Group
################################################################################
# Begin Group "SparcLite"

################################################################################
# Begin Source File

SOURCE="\bfd\coff-sparc.c"

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"coff-sparc.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/"coff-sparc.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\config" /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt\sparclite" /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "__WIN32__" /D\
 "_WINDOWS" /D "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_SPARCLITE" /D "_MBCS"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c\
  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "a29k"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\gdb\sparc-tdep.c"

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"sparc-tdep.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/"sparc-tdep.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\config" /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt\sparclite" /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "__WIN32__" /D\
 "_WINDOWS" /D "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_SPARCLITE" /D "_MBCS"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c\
  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "a29k"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\gdb\sparcl-tdep.c"

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"sparcl-tdep.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/"sparcl-tdep.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\config" /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt\sparclite" /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "__WIN32__" /D\
 "_WINDOWS" /D "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_SPARCLITE" /D "_MBCS"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c\
  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "a29k"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\opcodes\sparc-opc.c"

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"sparc-opc.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/"sparc-opc.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\config" /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt\sparclite" /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "__WIN32__" /D\
 "_WINDOWS" /D "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_SPARCLITE" /D "_MBCS"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c\
  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "a29k"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\opcodes\sparc-dis.c"

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"sparc-dis.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/"sparc-dis.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\config" /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt\sparclite" /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "__WIN32__" /D\
 "_WINDOWS" /D "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_SPARCLITE" /D "_MBCS"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c\
  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "a29k"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\bfd\cpu-sparc.c"

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"cpu-sparc.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/"cpu-sparc.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\config" /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt\sparclite" /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "__WIN32__" /D\
 "_WINDOWS" /D "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_SPARCLITE" /D "_MBCS"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c\
  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "a29k"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\bfd\aout32.c

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/aout32.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/aout32.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\config" /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt\sparclite" /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "__WIN32__" /D\
 "_WINDOWS" /D "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_SPARCLITE" /D "_MBCS"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c\
  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "a29k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\bfd\stab-syms.c"

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"stab-syms.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/"stab-syms.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\config" /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt\sparclite" /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "__WIN32__" /D\
 "_WINDOWS" /D "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_SPARCLITE" /D "_MBCS"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c\
  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "a29k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\bfd\sunos.c

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/sunos.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/sunos.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\config" /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt\sparclite" /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "__WIN32__" /D\
 "_WINDOWS" /D "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_SPARCLITE" /D "_MBCS"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c\
  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "a29k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\prebuilt\sparclite\version.c

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/version.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

$(INTDIR)/version.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\config" /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt\sparclite" /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "__WIN32__" /D\
 "_WINDOWS" /D "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_SPARCLITE" /D "_MBCS"\
 /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c\
  $(SOURCE) 

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "a29k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# End Group
################################################################################
# Begin Group "mips"

################################################################################
# Begin Source File

SOURCE="\gdb\remote-mips.c"

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"remote-mips.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 0

$(INTDIR)/"remote-mips.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt" /I "g:\gdb\mswin\prebuilt\mips" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /I "..\..\gdb\config" /D "_DEBUG" /D "WIN32" /D\
 "__WIN32__" /D "_WINDOWS" /D "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_MIPS"\
 /D "NEED_basename" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb"  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\bfd\elf32-mips.c"

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"elf32-mips.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 0

$(INTDIR)/"elf32-mips.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt" /I "g:\gdb\mswin\prebuilt\mips" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /I "..\..\gdb\config" /D "_DEBUG" /D "WIN32" /D\
 "__WIN32__" /D "_WINDOWS" /D "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_MIPS"\
 /D "NEED_basename" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb"  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\bfd\elf.c

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/elf.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 0

$(INTDIR)/elf.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt" /I "g:\gdb\mswin\prebuilt\mips" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /I "..\..\gdb\config" /D "_DEBUG" /D "WIN32" /D\
 "__WIN32__" /D "_WINDOWS" /D "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_MIPS"\
 /D "NEED_basename" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb"  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\bfd\elf32.c

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/elf32.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 0

$(INTDIR)/elf32.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt" /I "g:\gdb\mswin\prebuilt\mips" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /I "..\..\gdb\config" /D "_DEBUG" /D "WIN32" /D\
 "__WIN32__" /D "_WINDOWS" /D "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_MIPS"\
 /D "NEED_basename" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb"  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\bfd\cpu-mips.c"

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"cpu-mips.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 0

$(INTDIR)/"cpu-mips.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt" /I "g:\gdb\mswin\prebuilt\mips" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /I "..\..\gdb\config" /D "_DEBUG" /D "WIN32" /D\
 "__WIN32__" /D "_WINDOWS" /D "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_MIPS"\
 /D "NEED_basename" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb"  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\gdb\mips-tdep.c"

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"mips-tdep.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 0

$(INTDIR)/"mips-tdep.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt" /I "g:\gdb\mswin\prebuilt\mips" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /I "..\..\gdb\config" /D "_DEBUG" /D "WIN32" /D\
 "__WIN32__" /D "_WINDOWS" /D "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_MIPS"\
 /D "NEED_basename" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb"  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\bfd\ecofflink.c

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/ecofflink.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 0

$(INTDIR)/ecofflink.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt" /I "g:\gdb\mswin\prebuilt\mips" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /I "..\..\gdb\config" /D "_DEBUG" /D "WIN32" /D\
 "__WIN32__" /D "_WINDOWS" /D "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_MIPS"\
 /D "NEED_basename" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb"  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\bfd\coff-mips.c"

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"coff-mips.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 0

$(INTDIR)/"coff-mips.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt" /I "g:\gdb\mswin\prebuilt\mips" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /I "..\..\gdb\config" /D "_DEBUG" /D "WIN32" /D\
 "__WIN32__" /D "_WINDOWS" /D "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_MIPS"\
 /D "NEED_basename" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb"  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\bfd\ecoff.c

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/ecoff.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 0

$(INTDIR)/ecoff.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt" /I "g:\gdb\mswin\prebuilt\mips" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /I "..\..\gdb\config" /D "_DEBUG" /D "WIN32" /D\
 "__WIN32__" /D "_WINDOWS" /D "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_MIPS"\
 /D "NEED_basename" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb"  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\opcodes\mips-opc.c"

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"mips-opc.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 0

$(INTDIR)/"mips-opc.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt" /I "g:\gdb\mswin\prebuilt\mips" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /I "..\..\gdb\config" /D "_DEBUG" /D "WIN32" /D\
 "__WIN32__" /D "_WINDOWS" /D "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_MIPS"\
 /D "NEED_basename" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb"  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\opcodes\mips-dis.c"

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"mips-dis.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 0

$(INTDIR)/"mips-dis.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt" /I "g:\gdb\mswin\prebuilt\mips" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /I "..\..\gdb\config" /D "_DEBUG" /D "WIN32" /D\
 "__WIN32__" /D "_WINDOWS" /D "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_MIPS"\
 /D "NEED_basename" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb"  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\bfd\elflink.c

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/elflink.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 0

$(INTDIR)/elflink.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt" /I "g:\gdb\mswin\prebuilt\mips" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /I "..\..\gdb\config" /D "_DEBUG" /D "WIN32" /D\
 "__WIN32__" /D "_WINDOWS" /D "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_MIPS"\
 /D "NEED_basename" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb"  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\prebuilt\mips\version.c

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/version.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 0

$(INTDIR)/version.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt" /I "g:\gdb\mswin\prebuilt\mips" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /I "..\..\gdb\config" /D "_DEBUG" /D "WIN32" /D\
 "__WIN32__" /D "_WINDOWS" /D "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_MIPS"\
 /D "NEED_basename" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb"  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\gdb\remote-sim.c"

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"remote-sim.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/"remote-sim.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt" /I "g:\gdb\mswin\prebuilt\mips" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /I "..\..\gdb\config" /D "_DEBUG" /D "WIN32" /D\
 "__WIN32__" /D "_WINDOWS" /D "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_MIPS"\
 /D "NEED_basename" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb"  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\prebuilt\mips\interp.c

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/interp.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

$(INTDIR)/interp.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt" /I "g:\gdb\mswin\prebuilt\mips" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /I "..\..\gdb\config" /D "_DEBUG" /D "WIN32" /D\
 "__WIN32__" /D "_WINDOWS" /D "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_MIPS"\
 /D "NEED_basename" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb"  $(SOURCE) 

!ELSEIF  "$(CFG)" == "a29k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# End Group
################################################################################
# Begin Group "a29k"

################################################################################
# Begin Source File

SOURCE="\gdb\a29k-tdep.c"

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"a29k-tdep.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/"a29k-tdep.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt" /I "g:\gdb\mswin\prebuilt\a29k" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_A29K" /D "_MBCS"\
 /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\bfd\coff-a29k.c"

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"coff-a29k.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/"coff-a29k.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt" /I "g:\gdb\mswin\prebuilt\a29k" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_A29K" /D "_MBCS"\
 /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\bfd\cpu-a29k.c"

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"cpu-a29k.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/"cpu-a29k.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt" /I "g:\gdb\mswin\prebuilt\a29k" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_A29K" /D "_MBCS"\
 /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\gdb\29k-share\udi\udi2go32.c"

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/udi2go32.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/udi2go32.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt" /I "g:\gdb\mswin\prebuilt\a29k" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_A29K" /D "_MBCS"\
 /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\gdb\remote-udi.c"

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"remote-udi.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/"remote-udi.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt" /I "g:\gdb\mswin\prebuilt\a29k" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_A29K" /D "_MBCS"\
 /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\gdb\29k-share\udi\udr.c"

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/udr.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/udr.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt" /I "g:\gdb\mswin\prebuilt\a29k" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_A29K" /D "_MBCS"\
 /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\gdb\29k-share\udi\udip2soc.c"

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/udip2soc.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/udip2soc.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt" /I "g:\gdb\mswin\prebuilt\a29k" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_A29K" /D "_MBCS"\
 /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\opcodes\a29k-dis.c"

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"a29k-dis.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/"a29k-dis.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt" /I "g:\gdb\mswin\prebuilt\a29k" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_A29K" /D "_MBCS"\
 /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\prebuilt\a29k\version.c

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/version.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "a29k"

$(INTDIR)/version.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt" /I "g:\gdb\mswin\prebuilt\a29k" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_A29K" /D "_MBCS"\
 /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "i386"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# End Group
################################################################################
# Begin Group "i386"

################################################################################
# Begin Source File

SOURCE="\bfd\cpu-i386.c"

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"cpu-i386.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "a29k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP Exclude_From_Build 0

$(INTDIR)/"cpu-i386.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt" /I "g:\gdb\mswin\prebuilt\i386" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_I386" /D "_MBCS"\
 /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\gdb\i386-tdep.c"

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"i386-tdep.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "a29k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP Exclude_From_Build 0

$(INTDIR)/"i386-tdep.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt" /I "g:\gdb\mswin\prebuilt\i386" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_I386" /D "_MBCS"\
 /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\opcodes\i386-dis.c"

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"i386-dis.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "a29k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP Exclude_From_Build 0

$(INTDIR)/"i386-dis.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt" /I "g:\gdb\mswin\prebuilt\i386" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_I386" /D "_MBCS"\
 /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\bfd\aout32.c

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/aout32.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "a29k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP Exclude_From_Build 0

$(INTDIR)/aout32.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt" /I "g:\gdb\mswin\prebuilt\i386" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_I386" /D "_MBCS"\
 /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE="\bfd\stab-syms.c"

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/"stab-syms.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "a29k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP Exclude_From_Build 0

$(INTDIR)/"stab-syms.obj" :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt" /I "g:\gdb\mswin\prebuilt\i386" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_I386" /D "_MBCS"\
 /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\bfd\i386aout.c

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/i386aout.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "a29k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

# PROP Exclude_From_Build 0

$(INTDIR)/i386aout.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt" /I "g:\gdb\mswin\prebuilt\i386" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_I386" /D "_MBCS"\
 /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\prebuilt\i386\version.c

!IF  "$(CFG)" == "sh"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "Win32 Release"

$(INTDIR)/version.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /W3 /GX /Zi /YX /Od /I "prebuilt" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "_MBCS" /FR$(INTDIR)/ /Fp$(OUTDIR)/"gui.pch"\
 /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ELSEIF  "$(CFG)" == "h8300"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "m68k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "SparcLite"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "mips"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "a29k"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "i386"

$(INTDIR)/version.obj :  $(SOURCE)  $(INTDIR)
   $(CPP) /nologo /G4 /GX /Zi /YX /Od /I "g:\gdb\mswin" /I\
 "g:\gdb\mswin\prebuilt" /I "g:\gdb\mswin\prebuilt\i386" /I "..\..\mmalloc" /I\
 "prebuilt\libiberty" /I "..\..\bfd" /I "..\..\libiberty" /I "..\..\include" /I\
 "..\..\readline" /I "..\..\gdb" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "ALMOST_STDC" /D "NO_SYS_PARAM" /D "TARGET_I386" /D "_MBCS"\
 /Fp$(OUTDIR)/"gui.pch" /Fo$(INTDIR)/ /Fd$(OUTDIR)/"gui.pdb" /c  $(SOURCE) 

!ENDIF 

# End Source File
# End Group
# End Project
################################################################################
