#
# Makefile to build perl on Windowns NT using Microsoft NMAKE.
#
#
# This is set up to build a perl.exe that runs off a shared library
# (perl.dll).  Also makes individual DLLs for the XS extensions.
#

#
# Set these to wherever you want "nmake install" to put your
# newly built perl.
INST_DRV=c:
INST_TOP=$(INST_DRV)\perl

#
# uncomment one if you are using Visual C++ 2.x or Borland
# comment out both if you are using Visual C++ 4.x and above
#CCTYPE=MSVC20
CCTYPE=BORLAND

#
# uncomment next line if you want debug version of perl (big,slow)
#CFG=Debug

#
# set the install locations of the compiler include/libraries
#CCHOME = f:\msdev\vc
CCHOME = D:\bc5
CCINCDIR = $(CCHOME)\include
CCLIBDIR = $(CCHOME)\lib

#
# set this to point to cmd.exe (only needed if you use some
# alternate shell that doesn't grok cmd.exe style commands)
SHELL = g:\winnt\system32\cmd.exe

#
# set this to your email address (perl will guess a value from
# from your loginname and your hostname, which may not be right)
#EMAIL = 

##################### CHANGE THESE ONLY IF YOU MUST #####################

#
# Programs to compile, build .lib files and link
#

.USESHELL :

.IF "$(CCTYPE)" == "BORLAND"

CC = bcc32
LINK32 = tlink32
LIB32 = tlib
IMPLIB = implib

#
# Options
#
RUNTIME  = -D_RTLDLL
INCLUDES = -I.\include -I. -I.. -I$(CCINCDIR)
#PCHFLAGS = -H -H$(INTDIR)\bcmoduls.pch 
DEFINES  = -DWIN32 -DPERLDLL
SUBSYS   = console
LIBC = cw32mti.lib
LIBFILES = import32.lib $(LIBC) odbc32.lib odbccp32.lib

WINIOMAYBE =

.IF  "$(CFG)" == "Debug"
OPTIMIZE = -v $(RUNTIME)
LINK_DBG = -v
.ELSE
OPTIMIZE = -O $(RUNTIME)
LINK_DBG = 
.ENDIF

CFLAGS   = -w -tWM -tWD $(INCLUDES) $(DEFINES) $(PCHFLAGS) $(OPTIMIZE)
LINK_FLAGS  = $(LINK_DBG) -L$(CCLIBDIR)
OBJOUT_FLAG = -o

.ELSE

CC=cl.exe
LINK32=link.exe
LIB32=$(LINK32) -lib
#
# Options
#
.IF "$(RUNTIME)" == ""
RUNTIME  = -MD
.ENDIF
INCLUDES = -I.\include -I. -I..
#PCHFLAGS = -Fp$(INTDIR)\vcmoduls.pch -YX 
DEFINES  = -DWIN32 -D_CONSOLE -DPERLDLL
SUBSYS   = console

.IF "$(RUNTIME)" == "-MD"
LIBC = msvcrt.lib
WINIOMAYBE =
.ELSE
LIBC = libcmt.lib
WINIOMAYBE = win32io.obj
.ENDIF

.IF  "$(CFG)" == "Debug"
.IF "$(CCTYPE)" == "MSVC20"
OPTIMIZE = -Od $(RUNTIME) -Z7 -D_DEBUG
.ELSE
OPTIMIZE = -Od $(RUNTIME)d -Z7 -D_DEBUG
.ENDIF
LINK_DBG = -debug -pdb:none
.ELSE
.IF "$(CCTYPE)" == "MSVC20"
OPTIMIZE = -Od $(RUNTIME) -DNDEBUG
.ELSE
OPTIMIZE = -Od $(RUNTIME) -DNDEBUG
.ENDIF
LINK_DBG = -release
.ENDIF

# we don't add LIBC here, the compiler do it based on -MD/-MT
LIBFILES = oldnames.lib kernel32.lib user32.lib gdi32.lib \
	winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib \
	oleaut32.lib netapi32.lib uuid.lib wsock32.lib mpr.lib winmm.lib \
	version.lib odbc32.lib odbccp32.lib

CFLAGS   = -nologo -W3 $(INCLUDES) $(DEFINES) $(PCHFLAGS) $(OPTIMIZE)
LINK_FLAGS  = -nologo $(LIBFILES) $(LINK_DBG) -machine:I386
OBJOUT_FLAG = -Fo

.ENDIF

#################### do not edit below this line #######################
############# NO USER-SERVICEABLE PARTS BEYOND THIS POINT ##############

#
# Rules
# 
.SUFFIXES : 
.SUFFIXES : .c .obj .dll .lib .exe

.c.obj:
	$(CC) -c $(CFLAGS) $(OBJOUT_FLAG)$@ $<

.IF "$(CCTYPE)" == "BORLAND"

.obj.dll:
	$(LINK32) -Tpd -ap $(LINK_FLAGS) c0d32.obj $<,$@,,$(LIBFILES),$(*B).def
	$(IMPLIB) $(*B).lib $@
.ELSE

.obj.dll:
	$(LINK32) -dll -subsystem:windows -implib:$(*B).lib -def:$(*B).def \
	    -out:$@ $(LINK_FLAGS) $< $(LIBPERL)  

.ENDIF

#
INST_BIN=$(INST_TOP)\bin
INST_LIB=$(INST_TOP)\lib
INST_POD=$(INST_LIB)\pod
INST_HTML=$(INST_POD)\html
LIBDIR=..\lib
EXTDIR=..\ext
PODDIR=..\pod
EXTUTILSDIR=$(LIBDIR)\extutils

#
# various targets
PERLIMPLIB=..\perl.lib
MINIPERL=..\miniperl.exe
PERLDLL=..\perl.dll
PERLEXE=..\perl.exe
GLOBEXE=..\perlglob.exe
CONFIGPM=..\lib\Config.pm
MINIMOD=..\lib\ExtUtils\Miniperl.pm

PL2BAT=bin\pl2bat.pl
GLOBBAT = bin\perlglob.bat

.IF "$(CCTYPE)" == "BORLAND"

# Borland wildargs is incompatible with MS setargv
CFGSH_TMPL = config.bc
CFGH_TMPL = config_H.bc
# Borland's perl.exe will work on W95, so we don't make this

.ELSE

MAKE = nmake -nologo
CFGSH_TMPL = config.vc
CFGH_TMPL = config_H.vc
PERL95EXE=..\perl95.exe

.ENDIF

XCOPY=xcopy /f /r /i /d
RCOPY=xcopy /f /r /i /e /d
#NULL=

#
# filenames given to xsubpp must have forward slashes (since it puts
# full pathnames in #line strings)
XSUBPP=..\$(MINIPERL) -I..\..\lib ..\$(EXTUTILSDIR)\xsubpp -C++ -prototypes

CORE_C=	..\av.c		\
	..\deb.c	\
	..\doio.c	\
	..\doop.c	\
	..\dump.c	\
	..\globals.c	\
	..\gv.c		\
	..\hv.c		\
	..\mg.c		\
	..\op.c		\
	..\perl.c	\
	..\perlio.c	\
	..\perly.c	\
	..\pp.c		\
	..\pp_ctl.c	\
	..\pp_hot.c	\
	..\pp_sys.c	\
	..\regcomp.c	\
	..\regexec.c	\
	..\run.c	\
	..\scope.c	\
	..\sv.c		\
	..\taint.c	\
	..\toke.c	\
	..\universal.c	\
	..\util.c

CORE_OBJ= ..\av.obj	\
	..\deb.obj	\
	..\doio.obj	\
	..\doop.obj	\
	..\dump.obj	\
	..\globals.obj	\
	..\gv.obj	\
	..\hv.obj	\
	..\mg.obj	\
	..\op.obj	\
	..\perl.obj	\
	..\perlio.obj	\
	..\perly.obj	\
	..\pp.obj	\
	..\pp_ctl.obj	\
	..\pp_hot.obj	\
	..\pp_sys.obj	\
	..\regcomp.obj	\
	..\regexec.obj	\
	..\run.obj	\
	..\scope.obj	\
	..\sv.obj	\
	..\taint.obj	\
	..\toke.obj	\
	..\universal.obj\
	..\util.obj

WIN32_C = perllib.c \
	win32.c \
	win32io.c \
	win32sck.c

WIN32_OBJ = win32.obj \
	win32io.obj \
	win32sck.obj

PERL95_OBJ = perl95.obj \
	win32mt.obj \
	win32iomt.obj \
	win32sckmt.obj

DLL_OBJ = perllib.obj $(DYNALOADER).obj

CORE_H = ..\av.h	\
	..\cop.h	\
	..\cv.h		\
	..\dosish.h	\
	..\embed.h	\
	..\form.h	\
	..\gv.h		\
	..\handy.h	\
	..\hv.h		\
	..\mg.h		\
	..\nostdio.h	\
	..\op.h		\
	..\opcode.h	\
	..\perl.h	\
	..\perlio.h	\
	..\perlsdio.h	\
	..\perlsfio.h	\
	..\perly.h	\
	..\pp.h		\
	..\proto.h	\
	..\regexp.h	\
	..\scope.h	\
	..\sv.h		\
	..\unixish.h	\
	..\util.h	\
	..\XSUB.h	\
	.\config.h	\
	..\EXTERN.h	\
	.\include\dirent.h	\
	.\include\netdb.h	\
	.\include\sys\socket.h	\
	.\win32.h


EXTENSIONS=DynaLoader Socket IO Fcntl Opcode SDBM_File

DYNALOADER=$(EXTDIR)\DynaLoader\DynaLoader
SOCKET=$(EXTDIR)\Socket\Socket
FCNTL=$(EXTDIR)\Fcntl\Fcntl
OPCODE=$(EXTDIR)\Opcode\Opcode
SDBM_FILE=$(EXTDIR)\SDBM_File\SDBM_File
IO=$(EXTDIR)\IO\IO

SOCKET_DLL=..\lib\auto\Socket\Socket.dll
FCNTL_DLL=..\lib\auto\Fcntl\Fcntl.dll
OPCODE_DLL=..\lib\auto\Opcode\Opcode.dll
SDBM_FILE_DLL=..\lib\auto\SDBM_File\SDBM_File.dll
IO_DLL=..\lib\auto\IO\IO.dll

STATICLINKMODULES=DynaLoader
DYNALOADMODULES=	\
	$(SOCKET_DLL)	\
	$(FCNTL_DLL)	\
	$(OPCODE_DLL)	\
	$(SDBM_FILE_DLL)\
	$(IO_DLL)

POD2HTML=$(PODDIR)\pod2html
POD2MAN=$(PODDIR)\pod2man
POD2LATEX=$(PODDIR)\pod2latex
POD2TEXT=$(PODDIR)\pod2text

#
# Top targets
#

all: $(PERLEXE) $(PERL95EXE) $(GLOBEXE) $(DYNALOADMODULES) $(MINIMOD) $(GLOBBAT)

$(DYNALOADER).obj : $(DYNALOADER).c $(CORE_H) $(EXTDIR)\DynaLoader\dlutils.c

#------------------------------------------------------------

$(GLOBEXE): perlglob.obj
.IF "$(CCTYPE)" == "BORLAND"
	$(CC) -c -w -v -tWM -I$(CCINCDIR) perlglob.c
	$(LINK32) -Tpe -ap $(LINK_FLAGS) c0x32.obj perlglob.obj \
	    $(CCLIBDIR)\32BIT\wildargs.obj,$@,,import32.lib cw32mt.lib,
.ELSE
	$(LINK32) $(LINK_FLAGS) -out:$@ -subsystem:$(SUBSYS) perlglob.obj setargv.obj 
.ENDIF

$(GLOBBAT) : ..\lib\File\DosGlob.pm $(MINIPERL)
	$(MINIPERL) $(PL2BAT) - < ..\lib\File\DosGlob.pm > $(GLOBBAT)

perlglob.obj  : perlglob.c

..\miniperlmain.obj : ..\miniperlmain.c $(CORE_H)

config.w32 : $(CFGSH_TMPL)
	copy $(CFGSH_TMPL) config.w32

.\config.h : $(CFGSH_TMPL)
	-del /f config.h
	copy $(CFGH_TMPL) config.h

..\config.sh : config.w32 $(MINIPERL) config_sh.PL
	$(MINIPERL) -I..\lib config_sh.PL "INST_DRV=$(INST_DRV)" \
	    "INST_TOP=$(INST_TOP)" "cc=$(CC)" "ccflags=$(RUNTIME) -DWIN32" \
	    "cf_email=$(EMAIL)" "libs=$(LIBFILES:f)" "incpath=$(CCINCDIR)" \
	    "libpth=$(strip $(CCLIBDIR) $(LIBFILES:d))" "libc=$(LIBC)" \
	    config.w32 > ..\config.sh

$(CONFIGPM) : $(MINIPERL) ..\config.sh config_h.PL ..\minimod.pl
	cd .. && miniperl configpm
	if exist lib\* $(RCOPY) lib\*.* ..\lib\$(NULL)
	$(XCOPY) ..\*.h ..\lib\CORE\*.*
	$(XCOPY) *.h ..\lib\CORE\*.*
	$(RCOPY) include ..\lib\CORE\*.*
	$(MINIPERL) -I..\lib config_h.PL || $(MAKE) CCTYPE=$(CCTYPE) \
	    RUNTIME=$(RUNTIME) CFG=$(CFG) $(CONFIGPM)

$(MINIPERL) : ..\miniperlmain.obj $(CORE_OBJ) $(WIN32_OBJ)
.IF "$(CCTYPE)" == "BORLAND"
	$(LINK32) -Tpe -ap $(LINK_FLAGS) \
	    @$(mktmp c0x32.obj ..\miniperlmain.obj \
		$(CORE_OBJ:s,\,\\) $(WIN32_OBJ:s,\,\\),$@,,$(LIBFILES),)
.ELSE
	$(LINK32) -subsystem:console -out:$@ \
	    @$(mktmp $(LINK_FLAGS) ..\miniperlmain.obj \
		$(CORE_OBJ:s,\,\\) $(WIN32_OBJ:s,\,\\))
.ENDIF

$(WIN32_OBJ) : $(CORE_H)
$(CORE_OBJ)  : $(CORE_H)
$(DLL_OBJ)   : $(CORE_H) 

perldll.def : $(MINIPERL) $(CONFIGPM)
	$(MINIPERL) -w makedef.pl $(CCTYPE) > perldll.def

$(PERLDLL): perldll.def $(CORE_OBJ) $(WIN32_OBJ) $(DLL_OBJ)
.IF "$(CCTYPE)" == "BORLAND"
	$(LINK32) -Tpd -ap $(LINK_FLAGS) \
	    @$(mktmp c0d32.obj $(CORE_OBJ:s,\,\\) \
		$(WIN32_OBJ:s,\,\\) $(DLL_OBJ:s,\,\\)\n \
		$@,\n \
		$(LIBFILES)\n \
		perldll.def\n)
	$(IMPLIB) $*.lib $@
.ELSE
	$(LINK32) -dll -def:perldll.def -out:$@ \
	    @$(mktmp $(LINK_FLAGS) $(CORE_OBJ:s,\,\\) \
		$(WIN32_OBJ:s,\,\\) $(DLL_OBJ:s,\,\\))
.ENDIF
	$(XCOPY) $(PERLIMPLIB) ..\lib\CORE

perl.def  : $(MINIPERL) makeperldef.pl
	$(MINIPERL) -I..\lib makeperldef.pl $(NULL) > perl.def

$(MINIMOD) : $(MINIPERL) ..\minimod.pl
	cd .. && miniperl minimod.pl > lib\ExtUtils\Miniperl.pm

perlmain.c : runperl.c 
	copy runperl.c perlmain.c

perlmain.obj : perlmain.c
	$(CC) $(CFLAGS) -UPERLDLL -c perlmain.c


$(PERLEXE): $(PERLDLL) $(CONFIGPM) perlmain.obj  
.IF "$(CCTYPE)" == "BORLAND"
	$(LINK32) -Tpe -ap $(LINK_FLAGS) \
	    @$(mktmp c0x32.obj perlmain.obj $(WINIOMAYBE)\n \
	    $@,\n \
	    $(PERLIMPLIB) $(LIBFILES)\n)
.ELSE
	$(LINK32) -subsystem:console -out:perl.exe $(LINK_FLAGS) \
	    perlmain.obj $(WINIOMAYBE) $(PERLIMPLIB) 
	copy perl.exe $@
	del perl.exe
.ENDIF
	copy splittree.pl .. 
	$(MINIPERL) -I..\lib ..\splittree.pl "../LIB" "../LIB/auto"
	attrib -r ..\t\*.*
	copy test ..\t

.IF "$(CCTYPE)" != "BORLAND"

perl95.c : runperl.c 
	copy runperl.c perl95.c

perl95.obj : perl95.c
	$(CC) $(CFLAGS) -MT -UPERLDLL -c perl95.c

win32iomt.obj : win32io.c
	$(CC) $(CFLAGS) -MT -c $(OBJOUT_FLAG)win32iomt.obj win32io.c

win32sckmt.obj : win32sck.c
	$(CC) $(CFLAGS) -MT -c $(OBJOUT_FLAG)win32sckmt.obj win32sck.c

win32mt.obj : win32.c
	$(CC) $(CFLAGS) -MT -c $(OBJOUT_FLAG)win32mt.obj win32.c

$(PERL95EXE): $(PERLDLL) $(CONFIGPM) $(PERL95_OBJ)
	$(LINK32) -subsystem:console -out:perl95.exe $(LINK_FLAGS) \
	    $(PERL95_OBJ) $(PERLIMPLIB) 
	copy perl95.exe $@
	del perl95.exe

.ENDIF

$(DYNALOADER).c: $(MINIPERL) $(EXTDIR)\DynaLoader\dl_win32.xs $(CONFIGPM)
	if not exist ..\lib\auto mkdir ..\lib\auto
	$(XCOPY) $(EXTDIR)\$(*B)\$(*B).pm $(LIBDIR)\$(NULL)
	cd $(EXTDIR)\$(*B) && $(XSUBPP) dl_win32.xs > $(*B).c
	$(XCOPY) $(EXTDIR)\$(*B)\dlutils.c .

$(EXTDIR)\DynaLoader\dl_win32.xs: dl_win32.xs
	copy dl_win32.xs $(EXTDIR)\DynaLoader\dl_win32.xs

$(IO_DLL): $(PERLEXE) $(CONFIGPM) $(IO).xs
	cd $(EXTDIR)\$(*B) && \
	..\..\miniperl -I..\..\lib Makefile.PL INSTALLDIRS=perl
	cd $(EXTDIR)\$(*B) && $(MAKE)

$(SDBM_FILE_DLL) : $(PERLEXE) $(SDBM_FILE).xs
	cd $(EXTDIR)\$(*B) && \
	..\..\miniperl -I..\..\lib Makefile.PL INSTALLDIRS=perl
	cd $(EXTDIR)\$(*B) && $(MAKE)

$(FCNTL_DLL): $(PERLEXE) $(FCNTL).xs
	cd $(EXTDIR)\$(*B) && \
	..\..\miniperl -I..\..\lib Makefile.PL INSTALLDIRS=perl
	cd $(EXTDIR)\$(*B) && $(MAKE)

$(OPCODE_DLL): $(PERLEXE) $(OPCODE).xs
	cd $(EXTDIR)\$(*B) && \
	..\..\miniperl -I..\..\lib Makefile.PL INSTALLDIRS=perl
	cd $(EXTDIR)\$(*B) && $(MAKE)

$(SOCKET_DLL): $(SOCKET).xs $(PERLEXE)
	cd $(EXTDIR)\$(*B) && \
	..\..\miniperl -I..\..\lib Makefile.PL INSTALLDIRS=perl
	cd $(EXTDIR)\$(*B) && $(MAKE)

doc: $(PERLEXE)
	cd ..\pod && $(MAKE) -f ..\win32\pod.mak checkpods \
		pod2html pod2latex pod2man pod2text
	cd ..\pod && $(XCOPY) *.bat ..\win32\bin\*.*
	copy ..\README.win32 ..\pod\perlwin32.pod
	$(PERLEXE) ..\installhtml --podroot=.. --htmldir=./html \
	    --podpath=pod:lib:ext:utils --htmlroot="//$(INST_HTML:s,:,|,)" \
	    --libpod=perlfunc:perlguts:perlvar:perlrun:perlop --recurse

utils: $(PERLEXE)
	cd ..\utils && $(MAKE) PERL=$(MINIPERL)
	cd ..\utils && $(PERLEXE) ..\win32\$(PL2BAT) h2ph splain perlbug \
		pl2pm c2ph h2xs perldoc pstruct
	$(XCOPY) ..\utils\*.bat bin\*.*
	$(PERLEXE) $(PL2BAT) bin\network.pl bin\www.pl bin\runperl.pl \
			bin\pl2bat.pl

distclean: clean
	-del /f $(MINIPERL) $(PERLEXE) $(PERLDLL) $(GLOBEXE) \
		$(PERLIMPLIB) ..\miniperl.lib $(MINIMOD)
	-del /f *.def *.map
	-del /f $(SOCKET_DLL) $(IO_DLL) $(SDBM_FILE_DLL) $(FCNTL_DLL) \
		$(OPCODE_DLL)
	-del /f $(SOCKET).c $(IO).c $(SDBM_FILE).c $(FCNTL).c $(OPCODE).c \
		$(DYNALOADER).c
	-del /f $(PODDIR)\*.html
	-del /f $(PODDIR)\*.bat
	-del /f ..\config.sh ..\splittree.pl perlmain.c dlutils.c config.h.new
.IF "$(PERL95EXE)" != ""
	-del /f perl95.c
.ENDIF
	-del /f bin\*.bat
	-cd $(EXTDIR) && del /s *.lib *.def *.map *.bs Makefile *.obj pm_to_blib
	-rmdir /s /q ..\lib\auto
	-rmdir /s /q ..\lib\CORE

install : all doc utils
	if not exist $(INST_TOP) mkdir $(INST_TOP)
	echo I $(INST_TOP) L $(LIBDIR)
	$(XCOPY) $(PERLEXE) $(INST_BIN)\*.*
.IF "$(PERL95EXE)" != ""
	$(XCOPY) $(PERL95EXE) $(INST_BIN)\*.*
.ENDIF
	$(XCOPY) $(GLOBEXE) $(INST_BIN)\*.*
	$(XCOPY) $(PERLDLL) $(INST_BIN)\*.*
	$(XCOPY) bin\*.bat $(INST_BIN)\*.*
	$(RCOPY) ..\lib $(INST_LIB)\*.*
	$(XCOPY) ..\pod\*.bat $(INST_BIN)\*.*
	$(XCOPY) ..\pod\*.pod $(INST_POD)\*.*
	$(RCOPY) html\*.* $(INST_HTML)\*.*

inst_lib : $(CONFIGPM)
	copy splittree.pl .. 
	$(MINIPERL) -I..\lib ..\splittree.pl "../LIB" "../LIB/auto"
	$(RCOPY) ..\lib $(INST_LIB)\*.*

minitest : $(MINIPERL) $(GLOBEXE) $(CONFIGPM)
	$(XCOPY) $(MINIPERL) ..\t\perl.exe
.IF "$(CCTYPE)" == "BORLAND"
	$(XCOPY) $(GLOBBAT) ..\t\$(NULL)
.ELSE
	$(XCOPY) $(GLOBEXE) ..\t\$(NULL)
.ENDIF
	attrib -r ..\t\*.*
	copy test ..\t
	cd ..\t && \
	$(MINIPERL) -I..\lib test base/*.t comp/*.t cmd/*.t io/*.t op/*.t pragma/*.t

test : all
	$(XCOPY) $(PERLEXE) ..\t\$(NULL)
	$(XCOPY) $(PERLDLL) ..\t\$(NULL)
.IF "$(CCTYPE)" == "BORLAND"
	$(XCOPY) $(GLOBBAT) ..\t\$(NULL)
.ELSE
	$(XCOPY) $(GLOBEXE) ..\t\$(NULL)
.ENDIF
	cd ..\t && $(PERLEXE) -I..\lib harness

clean : 
	-@erase miniperlmain.obj
	-@erase $(MINIPERL)
	-@erase perlglob.obj
	-@erase perlmain.obj
	-@erase config.w32
	-@erase /f config.h
	-@erase $(GLOBEXE)
	-@erase $(PERLEXE)
	-@erase $(PERLDLL)
	-@erase $(CORE_OBJ)
	-@erase $(WIN32_OBJ)
	-@erase $(DLL_OBJ)
	-@erase ..\*.obj ..\*.lib ..\*.exp *.obj *.lib *.exp
	-@erase ..\t\*.exe ..\t\*.dll ..\t\*.bat
	-@erase *.ilk
	-@erase *.pdb


