#
# Makefile to build perl on Windows NT using DMAKE.
# Supported compilers:
#	Visual C++ 2.0 thro 5.0
#	Borland C++ 5.02
#	Mingw32 with gcc-2.8.1 or egcs-1.0.2  **experimental**
#
# This is set up to build a perl.exe that runs off a shared library
# (perl.dll).  Also makes individual DLLs for the XS extensions.
#

##
## Make sure you read README.win32 *before* you mess with anything here!
##

##
## Build configuration.  Edit the values below to suit your needs.
##

#
# Set these to wherever you want "nmake install" to put your
# newly built perl.
#
INST_DRV	*= c:
INST_TOP	*= $(INST_DRV)\perl

#
# Comment this out if you DON'T want your perl installation to be versioned.
# This means that the new installation will overwrite any files from the
# old installation at the same INST_TOP location.  Leaving it enabled is
# the safest route, as perl adds the extra version directory to all the
# locations it installs files to.  If you disable it, an alternative
# versioned installation can be obtained by setting INST_TOP above to a
# path that includes an arbitrary version string.
#
INST_VER	*= \5.00503

#
# uncomment to enable threads-capabilities
#
#USE_THREADS	*= define

#
# uncomment to enable multiple interpreters
#
#USE_MULTI	*= define

#
# uncomment one
#
#CCTYPE		*= MSVC20
#CCTYPE		*= MSVC
CCTYPE		*= BORLAND
#CCTYPE		*= GCC

#
# uncomment next line if you want to use the perl object
# Currently, this cannot be enabled if you ask for threads above, or
# if you are using GCC or EGCS.
#
#OBJECT		*= -DPERL_OBJECT

#
# uncomment next line if you want debug version of perl (big,slow)
#
#CFG		*= Debug

#
# uncomment next option if you want to use the VC++ compiler optimization.
# This option is only relevant for the Microsoft compiler; we automatically
# use maximum optimization with the other compilers (unless you specify a
# DEBUGGING build).
# Warning: This is known to produce incorrect code for compiler versions
# earlier than VC++ 98 (Visual Studio 6.0). VC++ 98 generates code that
# successfully passes the Perl regression test suite. It hasn't yet been
# widely tested with real applications though.
#
#CFG		*= Optimize

#
# uncomment to enable use of PerlCRT.DLL when using the Visual C compiler.
# Highly recommended.  It has patches that fix known bugs in MSVCRT.DLL.
# This currently requires VC 5.0 with Service Pack 3.
# Get it from CPAN at http://www.perl.com/CPAN/authors/id/D/DO/DOUGL/
# and follow the directions in the package to install.
#
#USE_PERLCRT	*= define

#
# uncomment to enable linking with setargv.obj under the Visual C
# compiler. Setting this options enables perl to expand wildcards in
# arguments, but it may be harder to use alternate methods like
# File::DosGlob that are more powerful.  This option is supported only with
# Visual C.
#
#USE_SETARGV	*= define

#
# if you have the source for des_fcrypt(), uncomment this and make sure the
# file exists (see README.win32).  File should be located in the same
# directory as this file.
#
#CRYPT_SRC	*= fcrypt.c

#
# if you didn't set CRYPT_SRC and if you have des_fcrypt() available in a
# library, uncomment this, and make sure the library exists (see README.win32)
# Specify the full pathname of the library.
#
#CRYPT_LIB	*= fcrypt.lib

#
# set this if you wish to use perl's malloc
# WARNING: Turning this on/off WILL break binary compatibility with extensions
# you may have compiled with/without it.  Be prepared to recompile all
# extensions if you change the default.  Currently, this cannot be enabled
# if you ask for PERL_OBJECT above.
#
#PERL_MALLOC	*= define

#
# set the install locations of the compiler include/libraries
# Running VCVARS32.BAT is *required* when using Visual C.
# Some versions of Visual C don't define MSVCDIR in the environment,
# so you may have to set CCHOME explicitly (spaces in the path name should
# not be quoted)
#
#CCHOME		*= f:\msdev\vc
CCHOME		*= C:\bc5
#CCHOME		*= D:\packages\mingw32
CCINCDIR	*= $(CCHOME)\include
CCLIBDIR	*= $(CCHOME)\lib

#
# specify semicolon-separated list of extra directories that modules will
# look for libraries (spaces in path names need not be quoted)
#
EXTRALIBDIRS	*=

#
# set this to point to cmd.exe (only needed if you use some
# alternate shell that doesn't grok cmd.exe style commands)
#
#SHELL		*= g:\winnt\system32\cmd.exe

#
# set this to your email address (perl will guess a value from
# from your loginname and your hostname, which may not be right)
#
#EMAIL		*= 

##
## Build configuration ends.
##

##################### CHANGE THESE ONLY IF YOU MUST #####################

.IF "$(CRYPT_SRC)$(CRYPT_LIB)" == ""
D_CRYPT		= undef
.ELSE
D_CRYPT		= define
CRYPT_FLAG	= -DHAVE_DES_FCRYPT
.ENDIF

.IF "$(OBJECT)" != ""
PERL_MALLOC	!= undef
USE_THREADS	!= undef
USE_MULTI	!= undef
.ENDIF

PERL_MALLOC	*= undef

USE_THREADS	*= undef
USE_MULTI	*= undef

#BUILDOPT	*= -DPERL_GLOBAL_STRUCT
# -DUSE_PERLIO -D__STDC__=1 -DUSE_SFIO -DI_SFIO -I\sfio97\include

.IMPORT .IGNORE : PROCESSOR_ARCHITECTURE

PROCESSOR_ARCHITECTURE *= x86

.IF "$(OBJECT)" != ""
ARCHNAME	= MSWin32-$(PROCESSOR_ARCHITECTURE)-object
.ELIF "$(USE_THREADS)" == "define"
ARCHNAME	= MSWin32-$(PROCESSOR_ARCHITECTURE)-thread
.ELSE
ARCHNAME	= MSWin32-$(PROCESSOR_ARCHITECTURE)
.ENDIF

ARCHDIR		= ..\lib\$(ARCHNAME)
COREDIR		= ..\lib\CORE
AUTODIR		= ..\lib\auto

#
# Programs to compile, build .lib files and link
#

.USESHELL :

.IF "$(CCTYPE)" == "BORLAND"

CC		= bcc32
LINK32		= tlink32
LIB32		= tlib /P128
IMPLIB		= implib -c

#
# Options
#
RUNTIME		= -D_RTLDLL
INCLUDES	= -I$(COREDIR) -I.\include -I. -I.. -I"$(CCINCDIR)"
#PCHFLAGS	= -H -Hc -H=c:\temp\bcmoduls.pch 
DEFINES		= -DWIN32 $(BUILDOPT) $(CRYPT_FLAG)
LOCDEFS		= -DPERLDLL -DPERL_CORE
SUBSYS		= console
CXX_FLAG	= -P

LIBC		= cw32mti.lib
LIBFILES	= $(CRYPT_LIB) import32.lib $(LIBC) odbc32.lib odbccp32.lib

.IF  "$(CFG)" == "Debug"
OPTIMIZE	= -v $(RUNTIME) -DDEBUGGING
LINK_DBG	= -v
.ELSE
OPTIMIZE	= -O2 $(RUNTIME)
LINK_DBG	= 
.ENDIF

CFLAGS		= -w -g0 -tWM -tWD $(INCLUDES) $(DEFINES) $(LOCDEFS) \
		$(PCHFLAGS) $(OPTIMIZE)
LINK_FLAGS	= $(LINK_DBG) -L"$(CCLIBDIR)"
OBJOUT_FLAG	= -o
EXEOUT_FLAG	= -e
LIBOUT_FLAG	= 

.ELIF "$(CCTYPE)" == "GCC"

CC		= gcc
LINK32		= gcc
LIB32		= ar rc
IMPLIB		= dlltool

o = .o
a = .a

#
# Options
#
RUNTIME		=
INCLUDES	= -I$(COREDIR) -I.\include -I. -I..
DEFINES		= -DWIN32 $(BUILDOPT) $(CRYPT_FLAG)
LOCDEFS		= -DPERLDLL -DPERL_CORE
SUBSYS		= console
CXX_FLAG	= -xc++

LIBC		= -lcrtdll
LIBFILES	= $(CRYPT_LIB) -ladvapi32 -luser32 -lnetapi32 -lwsock32 \
		-lmingw32 -lgcc -lmoldname $(LIBC) -lkernel32

.IF  "$(CFG)" == "Debug"
OPTIMIZE	= -g -O2 $(RUNTIME) -DDEBUGGING
LINK_DBG	= -g
.ELSE
OPTIMIZE	= -g -O2 $(RUNTIME)
LINK_DBG	= 
.ENDIF

CFLAGS		= $(INCLUDES) $(DEFINES) $(LOCDEFS) $(OPTIMIZE)
LINK_FLAGS	= $(LINK_DBG) -L"$(CCLIBDIR)"
OBJOUT_FLAG	= -o
EXEOUT_FLAG	= -o
LIBOUT_FLAG	= 

.ELSE

CC		= cl.exe
LINK32		= link.exe
LIB32		= $(LINK32) -lib

#
# Options
#

RUNTIME		= -MD
INCLUDES	= -I$(COREDIR) -I.\include -I. -I..
#PCHFLAGS	= -Fpc:\temp\vcmoduls.pch -YX 
DEFINES		= -DWIN32 -D_CONSOLE -DNO_STRICT $(BUILDOPT) $(CRYPT_FLAG)
LOCDEFS		= -DPERLDLL -DPERL_CORE
SUBSYS		= console
CXX_FLAG	= -TP -GX

.IF "$(USE_PERLCRT)" == ""
.IF  "$(CFG)" == "Debug"
PERLCRTLIBC	= msvcrtd.lib
.ELSE
PERLCRTLIBC	= msvcrt.lib
.ENDIF
.ELSE
.IF  "$(CFG)" == "Debug"
PERLCRTLIBC	= PerlCRTD.lib
.ELSE
PERLCRTLIBC	= PerlCRT.lib
.ENDIF
.ENDIF

.IF "$(RUNTIME)" == "-MD"
LIBC		= $(PERLCRTLIBC)
.ELSE
LIBC		= libcmt.lib
.ENDIF

.IF  "$(CFG)" == "Debug"
.IF "$(CCTYPE)" == "MSVC20"
OPTIMIZE	= -Od $(RUNTIME) -Z7 -D_DEBUG -DDEBUGGING
.ELSE
OPTIMIZE	= -Od $(RUNTIME)d -Zi -D_DEBUG -DDEBUGGING
.ENDIF
LINK_DBG	= -debug -pdb:none
.ELSE
.IF "$(CFG)" == "Optimize"
OPTIMIZE	= -O2 $(RUNTIME) -DNDEBUG
.ELSE
OPTIMIZE	= -Od $(RUNTIME) -DNDEBUG
.ENDIF
LINK_DBG	= -release
.ENDIF

LIBBASEFILES	= $(CRYPT_LIB) oldnames.lib kernel32.lib user32.lib gdi32.lib \
		winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib \
		oleaut32.lib netapi32.lib uuid.lib wsock32.lib mpr.lib winmm.lib \
		version.lib odbc32.lib odbccp32.lib

# we add LIBC here, since we may be using PerlCRT.dll
LIBFILES	= $(LIBBASEFILES) $(LIBC)

CFLAGS		= -nologo -Gf -W3 $(INCLUDES) $(DEFINES) $(LOCDEFS) \
		$(PCHFLAGS) $(OPTIMIZE)
LINK_FLAGS	= -nologo -nodefaultlib $(LINK_DBG) -machine:$(PROCESSOR_ARCHITECTURE)
OBJOUT_FLAG	= -Fo
EXEOUT_FLAG	= -Fe
LIBOUT_FLAG	= /out:

.ENDIF

.IF "$(OBJECT)" != ""
OPTIMIZE	+= $(CXX_FLAG)
.ENDIF

CFLAGS_O	= $(CFLAGS) $(OBJECT)

#################### do not edit below this line #######################
############# NO USER-SERVICEABLE PARTS BEYOND THIS POINT ##############

o *= .obj
a *= .lib

LKPRE		= INPUT (
LKPOST		= )

#
# Rules
# 

.SUFFIXES : .c $(o) .dll $(a) .exe 

.c$(o):
	$(CC) -c $(null,$(<:d) $(NULL) -I$(<:d)) $(CFLAGS_O) $(OBJOUT_FLAG)$@ $<

.y.c:
	$(NOOP)

$(o).dll:
.IF "$(CCTYPE)" == "BORLAND"
	$(LINK32) -Tpd -ap $(LINK_FLAGS) c0d32$(o) $<,$@,,$(LIBFILES),$(*B).def
	$(IMPLIB) $(*B).lib $@
.ELIF "$(CCTYPE)" == "GCC"
	$(LINK32) -o $@ $(LINK_FLAGS) $< $(LIBFILES)
	$(IMPLIB) -def $(*B).def $(*B).a $@
.ELSE
	$(LINK32) -dll -subsystem:windows -implib:$(*B).lib -def:$(*B).def \
	    -out:$@ $(LINK_FLAGS) $(LIBFILES) $< $(LIBPERL)  
.ENDIF

#
INST_BIN	= $(INST_TOP)$(INST_VER)\bin\$(ARCHNAME)
INST_SCRIPT	= $(INST_TOP)$(INST_VER)\bin
INST_LIB	= $(INST_TOP)$(INST_VER)\lib
INST_POD	= $(INST_LIB)\pod
INST_HTML	= $(INST_POD)\html
LIBDIR		= ..\lib
EXTDIR		= ..\ext
PODDIR		= ..\pod
EXTUTILSDIR	= $(LIBDIR)\extutils

#
# various targets
MINIPERL	= ..\miniperl.exe
MINIDIR		= .\mini
PERLEXE		= ..\perl.exe
GLOBEXE		= ..\perlglob.exe
CONFIGPM	= ..\lib\Config.pm
MINIMOD		= ..\lib\ExtUtils\Miniperl.pm
X2P		= ..\x2p\a2p.exe

PL2BAT		= bin\pl2bat.pl
GLOBBAT		= bin\perlglob.bat

UTILS		=			\
		..\utils\h2ph		\
		..\utils\splain		\
		..\utils\perlbug	\
		..\utils\pl2pm 		\
		..\utils\c2ph		\
		..\utils\h2xs		\
		..\utils\perldoc	\
		..\utils\pstruct	\
		..\utils\perlcc		\
		..\pod\checkpods	\
		..\pod\pod2html		\
		..\pod\pod2latex	\
		..\pod\pod2man		\
		..\pod\pod2text		\
		..\x2p\find2perl	\
		..\x2p\s2p		\
		bin\www.pl		\
		bin\runperl.pl		\
		bin\pl2bat.pl		\
		bin\perlglob.pl		\
		bin\search.pl

.IF "$(CCTYPE)" == "BORLAND"

CFGSH_TMPL	= config.bc
CFGH_TMPL	= config_H.bc

.ELIF "$(CCTYPE)" == "GCC"

CFGSH_TMPL	= config.gc
CFGH_TMPL	= config_H.gc
.IF "$(OBJECT)" == "-DPERL_OBJECT"
PERLIMPLIB	= ..\libperlcore$(a)
.ELSE
PERLIMPLIB	= ..\libperl$(a)
.ENDIF

.ELSE

CFGSH_TMPL	= config.vc
CFGH_TMPL	= config_H.vc
.IF "$(USE_PERLCRT)" == ""
PERL95EXE	= ..\perl95.exe
.ENDIF

.ENDIF

.IF "$(OBJECT)" == "-DPERL_OBJECT"
PERLIMPLIB	*= ..\perlcore$(a)
PERLDLL		= ..\perlcore.dll
CAPILIB		= $(COREDIR)\perlCAPI$(a)
.ELSE
PERLIMPLIB	*= ..\perl$(a)
PERLDLL		= ..\perl.dll
CAPILIB		=
.ENDIF

XCOPY		= xcopy /f /r /i /d
RCOPY		= xcopy /f /r /i /e /d
NOOP		= @echo

#
# filenames given to xsubpp must have forward slashes (since it puts
# full pathnames in #line strings)
XSUBPP		= ..\$(MINIPERL) -I..\..\lib ..\$(EXTUTILSDIR)\xsubpp \
		-C++ -prototypes

MICROCORE_SRC	=		\
		..\av.c		\
		..\byterun.c	\
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

.IF "$(PERL_MALLOC)" == "define"
EXTRACORE_SRC	+= ..\malloc.c
.ENDIF

.IF "$(OBJECT)" == ""
EXTRACORE_SRC	+= ..\perlio.c
.ENDIF

WIN32_SRC	=		\
		.\win32.c	\
		.\win32sck.c

.IF "$(USE_THREADS)" == "define"
WIN32_SRC	+= .\win32thread.c 
.ENDIF

.IF "$(CRYPT_SRC)" != ""
WIN32_SRC	+= .\$(CRYPT_SRC)
.ENDIF

PERL95_SRC	=		\
		perl95.c	\
		win32mt.c	\
		win32sckmt.c

.IF "$(CRYPT_SRC)" != ""
PERL95_SRC	+= .\$(CRYPT_SRC)
.ENDIF

DLL_SRC		= $(DYNALOADER).c


.IF "$(OBJECT)" == ""
DLL_SRC		+= perllib.c
.ENDIF

X2P_SRC		=		\
		..\x2p\a2p.c	\
		..\x2p\hash.c	\
		..\x2p\str.c	\
		..\x2p\util.c	\
		..\x2p\walk.c

CORE_NOCFG_H	=		\
		..\av.h		\
		..\byterun.h	\
		..\bytecode.h	\
		..\cop.h	\
		..\cv.h		\
		..\dosish.h	\
		..\embed.h	\
		..\form.h	\
		..\gv.h		\
		..\handy.h	\
		..\hv.h		\
		..\iperlsys.h	\
		..\mg.h		\
		..\nostdio.h	\
		..\op.h		\
		..\opcode.h	\
		..\perl.h	\
		..\perlsdio.h	\
		..\perlsfio.h	\
		..\perly.h	\
		..\pp.h		\
		..\proto.h	\
		..\regexp.h	\
		..\scope.h	\
		..\sv.h		\
		..\thread.h	\
		..\unixish.h	\
		..\util.h	\
		..\XSUB.h	\
		..\EXTERN.h	\
		..\perlvars.h	\
		..\intrpvar.h	\
		..\thrdvar.h	\
		.\include\dirent.h	\
		.\include\netdb.h	\
		.\include\sys\socket.h	\
		.\win32.h

CORE_H		= $(CORE_NOCFG_H) .\config.h

MICROCORE_OBJ	= $(MICROCORE_SRC:db:+$(o))
CORE_OBJ	= $(MICROCORE_OBJ) $(EXTRACORE_SRC:db:+$(o))
WIN32_OBJ	= $(WIN32_SRC:db:+$(o))
MINICORE_OBJ	= $(MINIDIR)\{$(MICROCORE_OBJ:f) miniperlmain$(o) perlio$(o)}
MINIWIN32_OBJ	= $(MINIDIR)\{$(WIN32_OBJ:f)}
MINI_OBJ	= $(MINICORE_OBJ) $(MINIWIN32_OBJ)
PERL95_OBJ	= $(PERL95_SRC:db:+$(o))
DLL_OBJ		= $(DLL_SRC:db:+$(o))
X2P_OBJ		= $(X2P_SRC:db:+$(o))

PERLDLL_OBJ	= $(CORE_OBJ)
PERLEXE_OBJ	= perlmain$(o)

.IF "$(OBJECT)" == ""
PERLDLL_OBJ	+= $(WIN32_OBJ) $(DLL_OBJ)
.ELSE
PERLEXE_OBJ	+= $(WIN32_OBJ) $(DLL_OBJ)
PERL95_OBJ	+= DynaLoadmt$(o)
.ENDIF

.IF "$(USE_SETARGV)" != ""
SETARGV_OBJ	= setargv$(o)
.ENDIF

DYNAMIC_EXT	= Socket IO Fcntl Opcode SDBM_File POSIX attrs Thread B re \
		Data/Dumper
STATIC_EXT	= DynaLoader
NONXS_EXT	= Errno

DYNALOADER	= $(EXTDIR)\DynaLoader\DynaLoader
SOCKET		= $(EXTDIR)\Socket\Socket
FCNTL		= $(EXTDIR)\Fcntl\Fcntl
OPCODE		= $(EXTDIR)\Opcode\Opcode
SDBM_FILE	= $(EXTDIR)\SDBM_File\SDBM_File
IO		= $(EXTDIR)\IO\IO
POSIX		= $(EXTDIR)\POSIX\POSIX
ATTRS		= $(EXTDIR)\attrs\attrs
THREAD		= $(EXTDIR)\Thread\Thread
B		= $(EXTDIR)\B\B
RE		= $(EXTDIR)\re\re
DUMPER		= $(EXTDIR)\Data\Dumper\Dumper
ERRNO		= $(EXTDIR)\Errno\Errno

SOCKET_DLL	= $(AUTODIR)\Socket\Socket.dll
FCNTL_DLL	= $(AUTODIR)\Fcntl\Fcntl.dll
OPCODE_DLL	= $(AUTODIR)\Opcode\Opcode.dll
SDBM_FILE_DLL	= $(AUTODIR)\SDBM_File\SDBM_File.dll
IO_DLL		= $(AUTODIR)\IO\IO.dll
POSIX_DLL	= $(AUTODIR)\POSIX\POSIX.dll
ATTRS_DLL	= $(AUTODIR)\attrs\attrs.dll
THREAD_DLL	= $(AUTODIR)\Thread\Thread.dll
B_DLL		= $(AUTODIR)\B\B.dll
DUMPER_DLL	= $(AUTODIR)\Data\Dumper\Dumper.dll
RE_DLL		= $(AUTODIR)\re\re.dll

ERRNO_PM	= $(LIBDIR)\Errno.pm

EXTENSION_C	=		\
		$(SOCKET).c	\
		$(FCNTL).c	\
		$(OPCODE).c	\
		$(SDBM_FILE).c	\
		$(IO).c		\
		$(POSIX).c	\
		$(ATTRS).c	\
		$(THREAD).c	\
		$(RE).c		\
		$(DUMPER).c	\
		$(B).c

EXTENSION_DLL	=		\
		$(SOCKET_DLL)	\
		$(FCNTL_DLL)	\
		$(OPCODE_DLL)	\
		$(SDBM_FILE_DLL)\
		$(IO_DLL)	\
		$(POSIX_DLL)	\
		$(ATTRS_DLL)	\
		$(DUMPER_DLL)	\
		$(B_DLL)

EXTENSION_PM	=		\
		$(ERRNO_PM)

# re.dll doesn't build with PERL_OBJECT yet
.IF "$(OBJECT)" == ""
EXTENSION_DLL	+=		\
		$(THREAD_DLL)	\
		$(RE_DLL)
.ENDIF

POD2HTML	= $(PODDIR)\pod2html
POD2MAN		= $(PODDIR)\pod2man
POD2LATEX	= $(PODDIR)\pod2latex
POD2TEXT	= $(PODDIR)\pod2text

CFG_VARS	=					\
		"INST_DRV=$(INST_DRV)"			\
		"INST_TOP=$(INST_TOP)"			\
		"INST_VER=$(INST_VER)"			\
		"archname=$(ARCHNAME)"			\
		"cc=$(CC)"				\
		"ccflags=$(OPTIMIZE:s/"/\"/) $(DEFINES) $(OBJECT)"	\
		"cf_email=$(EMAIL)"			\
		"d_crypt=$(D_CRYPT)"			\
		"d_mymalloc=$(PERL_MALLOC)"		\
		"libs=$(LIBFILES:f)"			\
		"incpath=$(CCINCDIR:s/"/\"/)"		\
		"libperl=$(PERLIMPLIB:f)"		\
		"libpth=$(CCLIBDIR:s/"/\"/);$(EXTRALIBDIRS:s/"/\"/)"	\
		"libc=$(LIBC)"				\
		"make=dmake"				\
		"_o=$(o)" "obj_ext=$(o)"		\
		"_a=$(a)" "lib_ext=$(a)"		\
		"static_ext=$(STATIC_EXT)"		\
		"dynamic_ext=$(DYNAMIC_EXT)"		\
		"nonxs_ext=$(NONXS_EXT)"		\
		"usethreads=$(USE_THREADS)"		\
		"usemultiplicity=$(USE_MULTI)"		\
		"LINK_FLAGS=$(LINK_FLAGS:s/"/\"/)"		\
		"optimize=$(OPTIMIZE:s/"/\"/)"

#
# Top targets
#

all : .\config.h $(GLOBEXE) $(MINIMOD) $(CONFIGPM) $(PERLEXE) $(PERL95EXE) \
	$(CAPILIB) $(X2P) $(EXTENSION_DLL) $(EXTENSION_PM)

$(DYNALOADER)$(o) : $(DYNALOADER).c $(CORE_H) $(EXTDIR)\DynaLoader\dlutils.c

#------------------------------------------------------------

$(GLOBEXE) : perlglob$(o)
.IF "$(CCTYPE)" == "BORLAND"
	$(CC) -c -w -v -tWM -I"$(CCINCDIR)" perlglob.c
	$(LINK32) -Tpe -ap $(LINK_FLAGS) c0x32$(o) perlglob$(o) \
	    "$(CCLIBDIR)\32BIT\wildargs$(o)",$@,,import32.lib cw32mt.lib,
.ELIF "$(CCTYPE)" == "GCC"
	$(LINK32) $(LINK_FLAGS) -o $@ perlglob$(o) $(LIBFILES)
.ELSE
	$(LINK32) $(LINK_FLAGS) $(LIBFILES) -out:$@ -subsystem:$(SUBSYS) \
	    perlglob$(o) setargv$(o) 
.ENDIF

perlglob$(o)  : perlglob.c

config.w32 : $(CFGSH_TMPL)
	copy $(CFGSH_TMPL) config.w32

.\config.h : $(CFGH_TMPL) $(CORE_NOCFG_H)
	-del /f config.h
	copy $(CFGH_TMPL) config.h

..\config.sh : config.w32 $(MINIPERL) config_sh.PL
	$(MINIPERL) -I..\lib config_sh.PL $(CFG_VARS) config.w32 > ..\config.sh

# this target is for when changes to the main config.sh happen
# edit config.{b,v,g}c and make this target once for each supported
# compiler (e.g. `dmake CCTYPE=BORLAND regen_config_h`)
regen_config_h:
	perl config_sh.PL $(CFG_VARS) $(CFGSH_TMPL) > ..\config.sh
	-cd .. && del /f perl.exe
	cd .. && perl configpm
	-del /f $(CFGH_TMPL)
	-mkdir $(COREDIR)
	-perl -I..\lib config_h.PL "INST_VER=$(INST_VER)"
	rename config.h $(CFGH_TMPL)

$(CONFIGPM) : $(MINIPERL) ..\config.sh config_h.PL ..\minimod.pl
	cd .. && miniperl configpm
	if exist lib\* $(RCOPY) lib\*.* ..\lib\$(NULL)
	$(XCOPY) ..\*.h $(COREDIR)\*.*
	$(XCOPY) *.h $(COREDIR)\*.*
	$(XCOPY) ..\ext\re\re.pm $(LIBDIR)\*.*
	$(RCOPY) include $(COREDIR)\*.*
	$(MINIPERL) -I..\lib config_h.PL "INST_VER=$(INST_VER)" \
	    || $(MAKE) $(MAKEMACROS) $(CONFIGPM)

$(MINIPERL) : $(MINIDIR) $(MINI_OBJ)
.IF "$(CCTYPE)" == "BORLAND"
	$(LINK32) -Tpe -ap $(LINK_FLAGS) \
	    @$(mktmp c0x32$(o) $(MINI_OBJ:s,\,\\),$(@:s,\,\\),,$(LIBFILES),)
.ELIF "$(CCTYPE)" == "GCC"
	$(LINK32) -v -o $@ $(LINK_FLAGS) \
	    $(mktmp $(LKPRE) $(MINI_OBJ:s,\,\\) $(LIBFILES) $(LKPOST))
.ELSE
	$(LINK32) -subsystem:console -out:$@ \
	    @$(mktmp $(LINK_FLAGS) $(LIBFILES) $(MINI_OBJ:s,\,\\))
.ENDIF

$(MINIDIR) :
	if not exist "$(MINIDIR)" mkdir "$(MINIDIR)"

$(MINICORE_OBJ) : $(CORE_NOCFG_H)
	$(CC) -c $(CFLAGS) $(OBJOUT_FLAG)$@ ..\$(*B).c

$(MINIWIN32_OBJ) : $(CORE_NOCFG_H)
	$(CC) -c $(CFLAGS) $(OBJOUT_FLAG)$@ $(*B).c

# 1. we don't want to rebuild miniperl.exe when config.h changes
# 2. we don't want to rebuild miniperl.exe with non-default config.h
$(MINI_OBJ)	: $(CORE_NOCFG_H)

$(WIN32_OBJ)	: $(CORE_H)
$(CORE_OBJ)	: $(CORE_H)
$(DLL_OBJ)	: $(CORE_H)
$(PERL95_OBJ)	: $(CORE_H)
$(X2P_OBJ)	: $(CORE_H)

perldll.def : $(MINIPERL) $(CONFIGPM) ..\global.sym makedef.pl
	$(MINIPERL) -w makedef.pl $(OPTIMIZE) $(DEFINES) $(OBJECT) \
	    CCTYPE=$(CCTYPE) > perldll.def

$(PERLDLL): perldll.def $(PERLDLL_OBJ)
.IF "$(CCTYPE)" == "BORLAND"
	$(LINK32) -Tpd -ap $(LINK_FLAGS) \
	    @$(mktmp c0d32$(o) $(PERLDLL_OBJ:s,\,\\)\n \
		$@,\n \
		$(LIBFILES)\n \
		perldll.def\n)
	$(IMPLIB) $*.lib $@
.ELIF "$(CCTYPE)" == "GCC"
	$(LINK32) -mdll -o $@ -Wl,--base-file -Wl,perl.base $(LINK_FLAGS) \
	    $(mktmp $(LKPRE) $(PERLDLL_OBJ:s,\,\\) $(LIBFILES) $(LKPOST))
	dlltool --output-lib $(PERLIMPLIB) \
                --dllname perl.dll \
                --def perldll.def \
                --base-file perl.base \
                --output-exp perl.exp
	$(LINK32) -mdll -o $@ $(LINK_FLAGS) \
	    $(mktmp $(LKPRE) $(PERLDLL_OBJ:s,\,\\) $(LIBFILES) \
		perl.exp $(LKPOST))
.ELSE
	$(LINK32) -dll -def:perldll.def -out:$@ \
	    @$(mktmp $(LINK_FLAGS) $(LIBFILES) $(PERLDLL_OBJ:s,\,\\))
.ENDIF
	$(XCOPY) $(PERLIMPLIB) $(COREDIR)

perl.def  : $(MINIPERL) makeperldef.pl
	$(MINIPERL) -I..\lib makeperldef.pl $(NULL) > perl.def

$(MINIMOD) : $(MINIPERL) ..\minimod.pl
	cd .. && miniperl minimod.pl > lib\ExtUtils\Miniperl.pm

..\x2p\a2p$(o) : ..\x2p\a2p.c
	$(CC) -I..\x2p $(CFLAGS) $(OBJOUT_FLAG)$@ -c ..\x2p\a2p.c

..\x2p\hash$(o) : ..\x2p\hash.c
	$(CC) -I..\x2p  $(CFLAGS) $(OBJOUT_FLAG)$@ -c ..\x2p\hash.c

..\x2p\str$(o) : ..\x2p\str.c
	$(CC) -I..\x2p  $(CFLAGS) $(OBJOUT_FLAG)$@ -c ..\x2p\str.c

..\x2p\util$(o) : ..\x2p\util.c
	$(CC) -I..\x2p  $(CFLAGS) $(OBJOUT_FLAG)$@ -c ..\x2p\util.c

..\x2p\walk$(o) : ..\x2p\walk.c
	$(CC) -I..\x2p  $(CFLAGS) $(OBJOUT_FLAG)$@ -c ..\x2p\walk.c

$(X2P) : $(MINIPERL) $(X2P_OBJ)
	$(MINIPERL) ..\x2p\find2perl.PL
	$(MINIPERL) ..\x2p\s2p.PL
.IF "$(CCTYPE)" == "BORLAND"
	$(LINK32) -Tpe -ap $(LINK_FLAGS) \
	    @$(mktmp c0x32$(o) $(X2P_OBJ:s,\,\\),$(@:s,\,\\),,$(LIBFILES),)
.ELIF "$(CCTYPE)" == "GCC"
	$(LINK32) -v -o $@ $(LINK_FLAGS) \
	    $(mktmp $(LKPRE) $(X2P_OBJ:s,\,\\) $(LIBFILES) $(LKPOST))
.ELSE
	$(LINK32) -subsystem:console -out:$@ \
	    @$(mktmp $(LINK_FLAGS) $(LIBFILES) $(X2P_OBJ:s,\,\\))
.ENDIF

perlmain.c : runperl.c 
	copy runperl.c perlmain.c

perlmain$(o) : perlmain.c
	$(CC) $(CFLAGS_O) -UPERLDLL $(OBJOUT_FLAG)$@ -c perlmain.c

$(PERLEXE): $(PERLDLL) $(CONFIGPM) $(PERLEXE_OBJ)
.IF "$(CCTYPE)" == "BORLAND"
	$(LINK32) -Tpe -ap $(LINK_FLAGS) \
	    @$(mktmp c0x32$(o) $(PERLEXE_OBJ:s,\,\\)\n \
	    $(@:s,\,\\),\n \
	    $(PERLIMPLIB) $(LIBFILES)\n)
.ELIF "$(CCTYPE)" == "GCC"
	$(LINK32) -o $@ $(LINK_FLAGS)  \
	    $(PERLEXE_OBJ) $(PERLIMPLIB) $(LIBFILES)
.ELSE
	$(LINK32) -subsystem:console -out:$@ $(LINK_FLAGS) $(LIBFILES) \
	    $(PERLEXE_OBJ) $(SETARGV_OBJ) $(PERLIMPLIB) 
.ENDIF
	copy splittree.pl .. 
	$(MINIPERL) -I..\lib ..\splittree.pl "../LIB" $(AUTODIR)

.IF "$(CCTYPE)" != "BORLAND"
.IF "$(CCTYPE)" != "GCC"
.IF "$(USE_PERLCRT)" == ""

perl95.c : runperl.c 
	copy runperl.c perl95.c

perl95$(o) : perl95.c
	$(CC) $(CFLAGS_O) -MT -UPERLDLL -DWIN95FIX -c perl95.c

win32sckmt$(o) : win32sck.c
	$(CC) $(CFLAGS_O) -MT -UPERLDLL -DWIN95FIX -c \
	    $(OBJOUT_FLAG)win32sckmt$(o) win32sck.c

win32mt$(o) : win32.c
	$(CC) $(CFLAGS_O) -MT -UPERLDLL -DWIN95FIX -c \
	    $(OBJOUT_FLAG)win32mt$(o) win32.c

DynaLoadmt$(o) : $(DYNALOADER).c
	$(CC) $(CFLAGS_O) -MT -UPERLDLL -DWIN95FIX -c \
	    $(OBJOUT_FLAG)DynaLoadmt$(o) $(DYNALOADER).c

$(PERL95EXE): $(PERLDLL) $(CONFIGPM) $(PERL95_OBJ)
	$(LINK32) -subsystem:console -nodefaultlib -out:$@ $(LINK_FLAGS) \
	    $(LIBBASEFILES) $(PERL95_OBJ) $(SETARGV_OBJ) $(PERLIMPLIB) \
	    libcmt.lib

.ENDIF
.ENDIF
.ENDIF

$(DYNALOADER).c: $(MINIPERL) $(EXTDIR)\DynaLoader\dl_win32.xs $(CONFIGPM)
	if not exist $(AUTODIR) mkdir $(AUTODIR)
	cd $(EXTDIR)\$(*B) && ..\$(MINIPERL) -I..\..\lib $(*B)_pm.PL
	$(XCOPY) $(EXTDIR)\$(*B)\$(*B).pm $(LIBDIR)\$(NULL)
	cd $(EXTDIR)\$(*B) && $(XSUBPP) dl_win32.xs > $(*B).c
	$(XCOPY) $(EXTDIR)\$(*B)\dlutils.c .

.IF "$(OBJECT)" == "-DPERL_OBJECT"

perlCAPI.cpp : $(MINIPERL)
	$(MINIPERL) GenCAPI.pl $(COREDIR)

perlCAPI$(o) : perlCAPI.cpp
.IF "$(CCTYPE)" == "BORLAND"
	$(CC) $(CFLAGS_O) -c $(OBJOUT_FLAG)perlCAPI$(o) perlCAPI.cpp
.ELIF "$(CCTYPE)" == "GCC"
	$(CC) $(CFLAGS_O) -c $(OBJOUT_FLAG)perlCAPI$(o) perlCAPI.cpp
.ELSE
	$(CC) $(CFLAGS_O) $(RUNTIME) -UPERLDLL -c \
	    $(OBJOUT_FLAG)perlCAPI$(o) perlCAPI.cpp
.ENDIF

$(CAPILIB) : perlCAPI.cpp perlCAPI$(o)
.IF "$(CCTYPE)" == "BORLAND"
	$(LIB32) $(LIBOUT_FLAG)$(CAPILIB) +perlCAPI$(o)
.ELSE
	$(LIB32) $(LIBOUT_FLAG)$(CAPILIB) perlCAPI$(o)
.ENDIF

.ENDIF

$(EXTDIR)\DynaLoader\dl_win32.xs: dl_win32.xs
	copy dl_win32.xs $(EXTDIR)\DynaLoader\dl_win32.xs

$(DUMPER_DLL): $(PERLEXE) $(DUMPER).xs
	cd $(EXTDIR)\Data\$(*B) && \
	..\..\..\miniperl -I..\..\..\lib Makefile.PL INSTALLDIRS=perl
	cd $(EXTDIR)\Data\$(*B) && $(MAKE)

$(RE_DLL): $(PERLEXE) $(RE).xs
	cd $(EXTDIR)\$(*B) && \
	..\..\miniperl -I..\..\lib Makefile.PL INSTALLDIRS=perl
	cd $(EXTDIR)\$(*B) && $(MAKE)

$(B_DLL): $(PERLEXE) $(B).xs
	cd $(EXTDIR)\$(*B) && \
	..\..\miniperl -I..\..\lib Makefile.PL INSTALLDIRS=perl
	cd $(EXTDIR)\$(*B) && $(MAKE)

$(THREAD_DLL): $(PERLEXE) $(THREAD).xs
	cd $(EXTDIR)\$(*B) && \
	..\..\miniperl -I..\..\lib Makefile.PL INSTALLDIRS=perl
	cd $(EXTDIR)\$(*B) && $(MAKE)

$(ATTRS_DLL): $(PERLEXE) $(ATTRS).xs
	cd $(EXTDIR)\$(*B) && \
	..\..\miniperl -I..\..\lib Makefile.PL INSTALLDIRS=perl
	cd $(EXTDIR)\$(*B) && $(MAKE)

$(POSIX_DLL): $(PERLEXE) $(POSIX).xs
	cd $(EXTDIR)\$(*B) && \
	..\..\miniperl -I..\..\lib Makefile.PL INSTALLDIRS=perl
	cd $(EXTDIR)\$(*B) && $(MAKE)

$(IO_DLL): $(PERLEXE) $(IO).xs
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

$(SOCKET_DLL): $(PERLEXE) $(SOCKET).xs
	cd $(EXTDIR)\$(*B) && \
	..\..\miniperl -I..\..\lib Makefile.PL INSTALLDIRS=perl
	cd $(EXTDIR)\$(*B) && $(MAKE)

$(ERRNO_PM): $(PERLEXE) $(ERRNO)_pm.PL
	cd $(EXTDIR)\$(*B) && \
	..\..\miniperl -I..\..\lib Makefile.PL INSTALLDIRS=perl
	cd $(EXTDIR)\$(*B) && $(MAKE)

doc: $(PERLEXE)
	$(PERLEXE) -I..\lib ..\installhtml --podroot=.. --htmldir=./html \
	    --podpath=pod:lib:ext:utils --htmlroot="file://$(INST_HTML:s,:,|,)"\
	    --libpod=perlfunc:perlguts:perlvar:perlrun:perlop --recurse

utils: $(PERLEXE) $(X2P)
	cd ..\utils && $(MAKE) PERL=$(MINIPERL)
	copy ..\README.win32 ..\pod\perlwin32.pod
	cd ..\pod && $(MAKE) -f ..\win32\pod.mak converters
	$(PERLEXE) $(PL2BAT) $(UTILS)

distclean: clean
	-del /f $(MINIPERL) $(PERLEXE) $(PERL95EXE) $(PERLDLL) $(GLOBEXE) \
		$(PERLIMPLIB) ..\miniperl$(a) $(MINIMOD)
	-del /f *.def *.map
	-del /f $(EXTENSION_DLL) $(EXTENSION_PM)
	-del /f $(EXTENSION_C) $(DYNALOADER).c $(ERRNO).pm
	-del /f $(EXTDIR)\DynaLoader\dl_win32.xs
	-del /f $(LIBDIR)\.exists $(LIBDIR)\attrs.pm $(LIBDIR)\DynaLoader.pm
	-del /f $(LIBDIR)\Fcntl.pm $(LIBDIR)\IO.pm $(LIBDIR)\Opcode.pm
	-del /f $(LIBDIR)\ops.pm $(LIBDIR)\Safe.pm $(LIBDIR)\Thread.pm
	-del /f $(LIBDIR)\SDBM_File.pm $(LIBDIR)\Socket.pm $(LIBDIR)\POSIX.pm
	-del /f $(LIBDIR)\B.pm $(LIBDIR)\O.pm $(LIBDIR)\re.pm
	-del /f $(LIBDIR)\Data\Dumper.pm
	-rmdir /s /q $(LIBDIR)\IO || rmdir /s $(LIBDIR)\IO
	-rmdir /s /q $(LIBDIR)\Thread || rmdir /s $(LIBDIR)\Thread
	-rmdir /s /q $(LIBDIR)\B || rmdir /s $(LIBDIR)\B
	-rmdir /s /q $(LIBDIR)\Data || rmdir /s $(LIBDIR)\Data
	-del /f $(PODDIR)\*.html
	-del /f $(PODDIR)\*.bat
	-cd ..\utils && del /f h2ph splain perlbug pl2pm c2ph h2xs perldoc \
	    pstruct *.bat
	-cd ..\x2p && del /f find2perl s2p *.bat
	-del /f ..\config.sh ..\splittree.pl perlmain.c dlutils.c config.h.new
	-del /f $(CONFIGPM)
.IF "$(PERL95EXE)" != ""
	-del /f perl95.c
.ENDIF
	-del /f bin\*.bat
	-cd $(EXTDIR) && del /s *$(a) *.def *.map *.pdb *.bs Makefile *$(o) \
	    pm_to_blib
	-rmdir /s /q $(AUTODIR) || rmdir /s $(AUTODIR)
	-rmdir /s /q $(COREDIR) || rmdir /s $(COREDIR)

install : all installbare installhtml

installbare : utils
	$(PERLEXE) ..\installperl
.IF "$(PERL95EXE)" != ""
	$(XCOPY) $(PERL95EXE) $(INST_BIN)\*.*
.ENDIF
	$(XCOPY) $(GLOBEXE) $(INST_BIN)\*.*
	$(XCOPY) bin\*.bat $(INST_SCRIPT)\*.*
	$(XCOPY) bin\network.pl $(INST_LIB)\*.*

installhtml : doc
	$(RCOPY) html\*.* $(INST_HTML)\*.*

inst_lib : $(CONFIGPM)
	copy splittree.pl .. 
	$(MINIPERL) -I..\lib ..\splittree.pl "../LIB" $(AUTODIR)
	$(RCOPY) ..\lib $(INST_LIB)\*.*

minitest : $(MINIPERL) $(GLOBEXE) $(CONFIGPM) utils
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

test-prep : all utils
	$(XCOPY) $(PERLEXE) ..\t\$(NULL)
	$(XCOPY) $(PERLDLL) ..\t\$(NULL)
.IF "$(CCTYPE)" == "BORLAND"
	$(XCOPY) $(GLOBBAT) ..\t\$(NULL)
.ELSE
	$(XCOPY) $(GLOBEXE) ..\t\$(NULL)
.ENDIF

test : test-prep
	cd ..\t && $(PERLEXE) -I..\lib harness

test-notty : test-prep
	set PERL_SKIP_TTY_TEST=1 && \
	cd ..\t && $(PERLEXE) -I.\lib harness

clean : 
	-@erase miniperlmain$(o)
	-@erase $(MINIPERL)
	-@erase perlglob$(o)
	-@erase perlmain$(o)
	-@erase perlCAPI.cpp
	-@erase config.w32
	-@erase /f config.h
	-@erase $(GLOBEXE)
	-@erase $(PERLEXE)
	-@erase $(PERLDLL)
	-@erase $(CORE_OBJ)
	-rmdir /s /q $(MINIDIR) || rmdir /s $(MINIDIR)
	-@erase $(WIN32_OBJ)
	-@erase $(DLL_OBJ)
	-@erase $(X2P_OBJ)
	-@erase ..\*$(o) ..\*$(a) ..\*.exp ..\*.res *$(o) *$(a) *.exp *.res
	-@erase ..\t\*.exe ..\t\*.dll ..\t\*.bat
	-@erase ..\x2p\*.exe ..\x2p\*.bat
	-@erase *.ilk
	-@erase *.pdb
