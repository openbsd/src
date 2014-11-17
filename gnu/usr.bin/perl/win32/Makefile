#
# Makefile to build perl on Windows using Microsoft NMAKE.
# Supported compilers:
#	Microsoft Visual C++ 6.0 or later
#	Windows SDK 64-bit compiler and tools
#
# This is set up to build a perl.exe that runs off a shared library
# (perl520.dll).  Also makes individual DLLs for the XS extensions.
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
INST_DRV	= c:
INST_TOP	= $(INST_DRV)\perl

#
# Uncomment if you want to build a 32-bit Perl using a 32-bit compiler
# on a 64-bit version of Windows.
#WIN64		= undef

#
# Comment this out if you DON'T want your perl installation to be versioned.
# This means that the new installation will overwrite any files from the
# old installation at the same INST_TOP location.  Leaving it enabled is
# the safest route, as perl adds the extra version directory to all the
# locations it installs files to.  If you disable it, an alternative
# versioned installation can be obtained by setting INST_TOP above to a
# path that includes an arbitrary version string.
#
#INST_VER	= \5.20.1

#
# Comment this out if you DON'T want your perl installation to have
# architecture specific components.  This means that architecture-
# specific files will be installed along with the architecture-neutral
# files.  Leaving it enabled is safer and more flexible, in case you
# want to build multiple flavors of perl and install them together in
# the same location.  Commenting it out gives you a simpler
# installation that is easier to understand for beginners.
#
#INST_ARCH	= \$(ARCHNAME)

#
# Uncomment this if you want perl to run
# 	$Config{sitelibexp}\sitecustomize.pl
# before anything else.  This script can then be set up, for example,
# to add additional entries to @INC.
#
#USE_SITECUST	= define

#
# uncomment to enable multiple interpreters.  This is needed for fork()
# emulation and for thread support, and is auto-enabled by USE_IMP_SYS
# and USE_ITHREADS below.
#
USE_MULTI	= define

#
# Interpreter cloning/threads; now reasonably complete.
# This should be enabled to get the fork() emulation.  This needs (and
# will auto-enable) USE_MULTI above.
#
USE_ITHREADS	= define

#
# uncomment to enable the implicit "host" layer for all system calls
# made by perl.  This is also needed to get fork().  This needs (and
# will auto-enable) USE_MULTI above.
#
USE_IMP_SYS	= define

#
# Comment out next assign to disable perl's I/O subsystem and use compiler's 
# stdio for IO - depending on your compiler vendor and run time library you may 
# then get a number of fails from make test i.e. bugs - complain to them not us ;-). 
# You will also be unable to take full advantage of perl5.8's support for multiple 
# encodings and may see lower IO performance. You have been warned.
USE_PERLIO	= define

#
# Comment this out if you don't want to enable large file support for
# some reason.  Should normally only be changed to maintain compatibility
# with an older release of perl.
USE_LARGE_FILES	= define

#
# Uncomment this if you're building a 32-bit perl and want 64-bit integers.
# (If you're building a 64-bit perl then you will have 64-bit integers whether
# or not this is uncommented.)
# Note: This option is not supported in 32-bit MSVC60 builds.
#USE_64_BIT_INT	= define

#
# uncomment exactly one of the following
#
# Visual C++ 6.x (aka Visual C++ 98)
CCTYPE		= MSVC60
# Visual C++ .NET 2002/2003 (aka Visual C++ 7.x) (full version)
#CCTYPE		= MSVC70
# Visual C++ Toolkit 2003 (aka Visual C++ 7.x) (free command-line tools)
#CCTYPE		= MSVC70FREE
# Windows Server 2003 SP1 Platform SDK (April 2005)
#CCTYPE		= SDK2003SP1
# Visual C++ 2005 (aka Visual C++ 8.x) (full version)
#CCTYPE		= MSVC80
# Visual C++ 2005 Express Edition (aka Visual C++ 8.x) (free version)
#CCTYPE		= MSVC80FREE
# Visual C++ 2008 (aka Visual C++ 9.x) (full version)
#CCTYPE		= MSVC90
# Visual C++ 2008 Express Edition (aka Visual C++ 9.x) (free version)
#CCTYPE		= MSVC90FREE
# Visual C++ 2010 (aka Visual C++ 10.x) (full version)
#CCTYPE		= MSVC100
# Visual C++ 2010 Express Edition (aka Visual C++ 10.x) (free version)
#CCTYPE		= MSVC100FREE
# Visual C++ 2012 (aka Visual C++ 11.x) (full version)
#CCTYPE		= MSVC110
# Visual C++ 2012 Express Edition (aka Visual C++ 11.x) (free version)
#CCTYPE		= MSVC110FREE
# Visual C++ 2013 (aka Visual C++ 12.x) (full version)
#CCTYPE		= MSVC120
# Visual C++ 2013 Express Edition (aka Visual C++ 12.x) (free version)
#CCTYPE		= MSVC120FREE

#
# If you are using Intel C++ Compiler uncomment this
#
#__ICC		= define

#
# uncomment next line if you want debug version of perl (big,slow)
# If not enabled, we automatically try to use maximum optimization
# with all compilers that are known to have a working optimizer.
#
#CFG		= Debug

#
# uncomment to enable linking with setargv.obj under the Visual C
# compiler. Setting this options enables perl to expand wildcards in
# arguments, but it may be harder to use alternate methods like
# File::DosGlob that are more powerful.  This option is supported only with
# Visual C.
#
#USE_SETARGV	= define

#
# set this if you wish to use perl's malloc
# WARNING: Turning this on/off WILL break binary compatibility with extensions
# you may have compiled with/without it.  Be prepared to recompile all
# extensions if you change the default.  Currently, this cannot be enabled
# if you ask for USE_IMP_SYS above.
#
#PERL_MALLOC	= define

#
# set this to enable debugging mstats
# This must be enabled to use the Devel::Peek::mstat() function.  This cannot
# be enabled without PERL_MALLOC as well.
#
#DEBUG_MSTATS	= define

#
# set this to additionally provide a statically linked perl-static.exe.
# Note that dynamic loading will not work with this perl, so you must
# include required modules statically using the STATIC_EXT or ALL_STATIC
# variables below. A static library perl520s.lib will also be created.
# Ordinary perl.exe is not affected by this option.
#
#BUILD_STATIC	= define

#
# in addition to BUILD_STATIC the option ALL_STATIC makes *every*
# extension get statically built
# This will result in a very large perl executable, but the main purpose
# is to have proper linking set so as to be able to create miscellaneous
# executables with different built-in extensions
#
#ALL_STATIC	= define

#
#
# set the install locations of the compiler include/libraries
# Running VCVARS32.BAT is *required* when using Visual C.
# Some versions of Visual C don't define MSVCDIR in the environment,
# so you may have to set CCHOME explicitly (spaces in the path name should
# not be quoted)
#
CCHOME		= $(MSVCDIR)
CCINCDIR	= $(CCHOME)\include
CCLIBDIR	= $(CCHOME)\lib

#
# Additional compiler flags can be specified here.
#
BUILDOPT	= $(BUILDOPTEXTRA)

#
# This should normally be disabled.  Enabling it will disable the File::Glob
# implementation of CORE::glob.
#
#BUILDOPT	= $(BUILDOPT) -DPERL_EXTERNAL_GLOB

#
# Perl needs to read scripts in text mode so that the DATA filehandle
# works correctly with seek() and tell(), or around auto-flushes of
# all filehandles (e.g. by system(), backticks, fork(), etc).
#
# The current version on the ByteLoader module on CPAN however only
# works if scripts are read in binary mode.  But before you disable text
# mode script reading (and break some DATA filehandle functionality)
# please check first if an updated ByteLoader isn't available on CPAN.
#
BUILDOPT	= $(BUILDOPT) -DPERL_TEXTMODE_SCRIPTS

#
# specify semicolon-separated list of extra directories that modules will
# look for libraries (spaces in path names need not be quoted)
#
EXTRALIBDIRS	=

#
# set this to your email address (perl will guess a value from
# from your loginname and your hostname, which may not be right)
#
#EMAIL		=

##
## Build configuration ends.
##

##################### CHANGE THESE ONLY IF YOU MUST #####################

!IF "$(USE_IMP_SYS)" == "define"
PERL_MALLOC	= undef
DEBUG_MSTATS	= undef
!ENDIF

!IF "$(PERL_MALLOC)" == ""
PERL_MALLOC	= undef
DEBUG_MSTATS	= undef
!ENDIF

!IF "$(DEBUG_MSTATS)" == ""
DEBUG_MSTATS	= undef
!ENDIF

!IF "$(DEBUG_MSTATS)" == "define"
BUILDOPT	= $(BUILDOPT) -DPERL_DEBUGGING_MSTATS
!ENDIF

!IF "$(USE_SITECUST)" == ""
USE_SITECUST	= undef
!ENDIF

!IF "$(USE_MULTI)" == ""
USE_MULTI	= undef
!ENDIF

!IF "$(USE_ITHREADS)" == ""
USE_ITHREADS	= undef
!ENDIF

!IF "$(USE_IMP_SYS)" == ""
USE_IMP_SYS	= undef
!ENDIF

!IF "$(USE_PERLIO)" == ""
USE_PERLIO	= undef
!ENDIF

!IF "$(USE_LARGE_FILES)" == ""
USE_LARGE_FILES	= undef
!ENDIF

!IF "$(USE_64_BIT_INT)" == ""
USE_64_BIT_INT	= undef
!ENDIF

!IF "$(USE_IMP_SYS)$(USE_MULTI)" == "defineundef"
USE_MULTI	= define
!ENDIF

!IF "$(USE_ITHREADS)$(USE_MULTI)" == "defineundef"
USE_MULTI	= define
!ENDIF

!IF "$(USE_SITECUST)" == "define"
BUILDOPT	= $(BUILDOPT) -DUSE_SITECUSTOMIZE
!ENDIF

!IF "$(USE_MULTI)" != "undef"
BUILDOPT	= $(BUILDOPT) -DPERL_IMPLICIT_CONTEXT
!ENDIF

!IF "$(USE_IMP_SYS)" != "undef"
BUILDOPT	= $(BUILDOPT) -DPERL_IMPLICIT_SYS
!ENDIF

!IF "$(PROCESSOR_ARCHITECTURE)" == ""
PROCESSOR_ARCHITECTURE	= x86
!ENDIF

!IF "$(WIN64)" == ""
# When we are running from a 32bit cmd.exe on AMD64 then
# PROCESSOR_ARCHITECTURE is set to x86 and PROCESSOR_ARCHITEW6432
# is set to AMD64
!IF "$(PROCESSOR_ARCHITEW6432)" != ""
PROCESSOR_ARCHITECTURE	= $(PROCESSOR_ARCHITEW6432)
WIN64			= define
!ELSE
!IF "$(PROCESSOR_ARCHITECTURE)" == "AMD64" || "$(PROCESSOR_ARCHITECTURE)" == "IA64"
WIN64			= define
!ELSE
WIN64			= undef
!ENDIF
!ENDIF
!ENDIF

!IF "$(WIN64)" == "define"
USE_64_BIT_INT	= define
!ENDIF

# Treat 64-bit MSVC60 (doesn't really exist) as SDK2003SP1 because
# both link against MSVCRT.dll (which is part of Windows itself) and
# not against a compiler specific versioned runtime.
!IF "$(WIN64)" == "define" && "$(CCTYPE)" == "MSVC60"
CCTYPE		= SDK2003SP1
!ENDIF

# Disable the 64-bit-int option for (32-bit) MSVC60 builds since that compiler
# does not support it.
!IF "$(CCTYPE)" == "MSVC60"
!UNDEF USE_64_BIT_INT
USE_64_BIT_INT	= undef
!ENDIF

# Most relevant compiler-specific options fall into two groups:
# either pre-MSVC80 or MSVC80 onwards, so define a macro for this.
!IF "$(CCTYPE)" == "MSVC60" || \
    "$(CCTYPE)" == "MSVC70" || "$(CCTYPE)" == "MSVC70FREE"
PREMSVC80	= define
!ELSE
PREMSVC80	= undef
!ENDIF

ARCHITECTURE = $(PROCESSOR_ARCHITECTURE)
!IF "$(ARCHITECTURE)" == "AMD64"
ARCHITECTURE	= x64
!ENDIF
!IF "$(ARCHITECTURE)" == "IA64"
ARCHITECTURE	= ia64
!ENDIF

!IF "$(USE_MULTI)" == "define"
ARCHNAME	= MSWin32-$(ARCHITECTURE)-multi
!ELSE
!IF "$(USE_PERLIO)" == "define"
ARCHNAME	= MSWin32-$(ARCHITECTURE)-perlio
!ELSE
ARCHNAME	= MSWin32-$(ARCHITECTURE)
!ENDIF
!ENDIF

!IF "$(USE_PERLIO)" == "define"
BUILDOPT	= $(BUILDOPT) -DUSE_PERLIO
!ENDIF

!IF "$(USE_ITHREADS)" == "define"
ARCHNAME	= $(ARCHNAME)-thread
!ENDIF

!IF "$(WIN64)" != "define"
!IF "$(USE_64_BIT_INT)" == "define"
ARCHNAME	= $(ARCHNAME)-64int
!ENDIF
!ENDIF

# All but the free version of VC++ 7.x can load DLLs on demand.  Makes the test
# suite run in about 10% less time.
!IF "$(CCTYPE)" != "MSVC70FREE"
DELAYLOAD	= -DELAYLOAD:ws2_32.dll delayimp.lib
!ENDIF

# Visual C++ 2005 and 2008 (VC++ 8.x and 9.x) create manifest files for EXEs and
# DLLs. These either need copying everywhere with the binaries, or else need
# embedding in them otherwise MSVCR80.dll or MSVCR90.dll won't be found. For
# simplicity, embed them if they exist (and delete them afterwards so that they
# don't get installed too).
EMBED_EXE_MANI	= if exist $@.manifest mt -nologo -manifest $@.manifest -outputresource:$@;1 && \
		  if exist $@.manifest del $@.manifest
EMBED_DLL_MANI	= if exist $@.manifest mt -nologo -manifest $@.manifest -outputresource:$@;2 && \
		  if exist $@.manifest del $@.manifest

ARCHDIR		= ..\lib\$(ARCHNAME)
COREDIR		= ..\lib\CORE
AUTODIR		= ..\lib\auto
LIBDIR		= ..\lib
EXTDIR		= ..\ext
DISTDIR		= ..\dist
CPANDIR		= ..\cpan
PODDIR		= ..\pod
HTMLDIR		= .\html

#
INST_SCRIPT	= $(INST_TOP)$(INST_VER)\bin
INST_BIN	= $(INST_SCRIPT)$(INST_ARCH)
INST_LIB	= $(INST_TOP)$(INST_VER)\lib
INST_ARCHLIB	= $(INST_LIB)$(INST_ARCH)
INST_COREDIR	= $(INST_ARCHLIB)\CORE
INST_HTML	= $(INST_TOP)$(INST_VER)\html

#
# Programs to compile, build .lib files and link
#

!IF "$(__ICC)" != "define"
CC		= cl
LINK32		= link
!ELSE
CC		= icl
LINK32		= xilink
!ENDIF
LIB32		= $(LINK32) -lib
RSC		= rc

#
# Options
#

INCLUDES	= -I$(COREDIR) -I.\include -I. -I..
#PCHFLAGS	= -Fpc:\temp\vcmoduls.pch -YX
DEFINES		= -DWIN32 -D_CONSOLE -DNO_STRICT
LOCDEFS		= -DPERLDLL -DPERL_CORE
SUBSYS		= console
CXX_FLAG	= -TP -EHsc

LIBC	= msvcrt.lib

!IF  "$(CFG)" == "Debug"
OPTIMIZE	= -Od -MD -Zi -DDEBUGGING
LINK_DBG	= -debug
!ELSE
# -O1 yields smaller code, which turns out to be faster than -O2 on x86 and x64
OPTIMIZE	= -O1 -MD -Zi -DNDEBUG
# we enable debug symbols in release builds also
LINK_DBG	= -debug -opt:ref,icf
# you may want to enable this if you want COFF symbols in the executables
# in addition to the PDB symbols.  The default Dr. Watson that ships with
# Windows can use the the former but not latter.  The free WinDbg can be
# installed to get better stack traces from just the PDB symbols, so we
# avoid the bloat of COFF symbols by default.
#LINK_DBG	= $(LINK_DBG) -debugtype:both
!  IF "$(CCTYPE)" != "MSVC60"
# enable Whole Program Optimizations (WPO) and Link Time Code Generation (LTCG)
OPTIMIZE	= $(OPTIMIZE) -GL
LINK_DBG	= $(LINK_DBG) -ltcg
LIB_FLAGS	= -ltcg
!  ENDIF
!ENDIF

!IF "$(WIN64)" == "define"
DEFINES		= $(DEFINES) -DWIN64 -DCONSERVATIVE
OPTIMIZE	= $(OPTIMIZE) -fp:precise
!ENDIF

# For now, silence warnings from VC++ 8.x onwards about "unsafe" CRT functions
# and POSIX CRT function names being deprecated.
!IF "$(PREMSVC80)" == "undef"
DEFINES		= $(DEFINES) -D_CRT_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_DEPRECATE
!ENDIF

# In VS 2005 (VC++ 8.0) Microsoft changes time_t from 32-bit to
# 64-bit, even in 32-bit mode.  It also provides the _USE_32BIT_TIME_T
# preprocessor option to revert back to the old functionality for
# backward compatibility.  We define this symbol here for older 32-bit
# compilers only (which aren't using it at all) for the sole purpose
# of getting it into $Config{ccflags}.  That way if someone builds
# Perl itself with e.g. VC6 but later installs an XS module using VC8
# the time_t types will still be compatible.
!IF "$(WIN64)" == "undef"
!  IF "$(PREMSVC80)" == "define"
BUILDOPT	= $(BUILDOPT) -D_USE_32BIT_TIME_T
!  ENDIF
!ENDIF

LIBBASEFILES	= \
		oldnames.lib kernel32.lib user32.lib gdi32.lib winspool.lib \
		comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib \
		netapi32.lib uuid.lib ws2_32.lib mpr.lib winmm.lib \
		version.lib odbc32.lib odbccp32.lib comctl32.lib

# Avoid __intel_new_proc_init link error for libircmt.
# libmmd is /MD equivelent, other variants exist.
# libmmd is Intel C's math addon funcs to MS CRT, contains long doubles, C99,
# and optimized C89 funcs
!IF "$(__ICC)" == "define"
LIBBASEFILES	= $(LIBBASEFILES) libircmt.lib libmmd.lib
!ENDIF

# The 64 bit Windows Server 2003 SP1 SDK compilers link against MSVCRT.dll, which
# doesn't include the buffer overrun verification code used by the /GS switch.
# Since the code links against libraries that are compiled with /GS, this
# "security cookie verification" code must be included via bufferoverflow.lib.
!IF "$(WIN64)" == "define" && "$(CCTYPE)" == "SDK2003SP1"
LIBBASEFILES    = $(LIBBASEFILES) bufferoverflowU.lib
!ENDIF

LIBFILES	= $(LIBBASEFILES) $(LIBC)

#EXTRACFLAGS	= -nologo -GF -W4 -wd4127 -wd4706
EXTRACFLAGS	= -nologo -GF -W3
CFLAGS		= $(EXTRACFLAGS) $(INCLUDES) $(DEFINES) $(LOCDEFS) \
		$(PCHFLAGS) $(OPTIMIZE)
LINK_FLAGS	= -nologo -nodefaultlib $(LINK_DBG) \
		-libpath:"$(INST_COREDIR)" \
		-machine:$(PROCESSOR_ARCHITECTURE)
LIB_FLAGS	= $(LIB_FLAGS) -nologo
OBJOUT_FLAG	= -Fo
EXEOUT_FLAG	= -Fe

CFLAGS_O	= $(CFLAGS) $(BUILDOPT)

!IF "$(PREMSVC80)" == "undef"
LINK_FLAGS	= $(LINK_FLAGS) "/manifestdependency:type='Win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'"
!ELSE
RSC_FLAGS	= -DINCLUDE_MANIFEST
!ENDIF


#################### do not edit below this line #######################
############# NO USER-SERVICEABLE PARTS BEYOND THIS POINT ##############

o = .obj

#
# Rules
#

.SUFFIXES : .c $(o) .dll .lib .exe .rc .res

.c$(o):
	$(CC) -c -I$(<D) $(CFLAGS_O) $(OBJOUT_FLAG)$@ $<

.c.i:
	$(CC) -c -I$(<D) $(CFLAGS_O) -P $(OBJOUT_FLAG)$@ $<

.y.c:
	$(NOOP)

$(o).dll:
	$(LINK32) -dll -subsystem:windows -implib:$(*B).lib -def:$(*B).def \
	    -out:$@ $(LINK_FLAGS) $(LIBFILES) $< $(LIBPERL)
	$(EMBED_DLL_MANI)

.rc.res:
	$(RSC) -i.. $(RSC_FLAGS) $<

#
# various targets

# makedef.pl must be updated if this changes, and this should normally
# only change when there is an incompatible revision of the public API.
PERLIMPLIB	= ..\perl520.lib
PERLSTATICLIB	= ..\perl520s.lib
PERLDLL		= ..\perl520.dll

MINIPERL	= ..\miniperl.exe
MINIDIR		= .\mini
PERLEXE		= ..\perl.exe
WPERLEXE	= ..\wperl.exe
PERLEXESTATIC	= ..\perl-static.exe
GLOBEXE		= ..\perlglob.exe
CONFIGPM	= ..\lib\Config.pm ..\lib\Config_heavy.pl
X2P		= ..\x2p\a2p.exe
GENUUDMAP	= ..\generate_uudmap.exe
!IF "$(BUILD_STATIC)" == "define" || "$(ALL_STATIC)" == "define"
PERLSTATIC	= static
!ELSE
PERLSTATIC	= 
!ENDIF

# Unicode data files generated by mktables
FIRSTUNIFILE     = ..\lib\unicore\Decomposition.pl
UNIDATAFILES	 = ..\lib\unicore\Decomposition.pl \
		   ..\lib\unicore\CombiningClass.pl ..\lib\unicore\Name.pl \
		   ..\lib\unicore\Heavy.pl ..\lib\unicore\mktables.lst     \
		   ..\lib\unicore\UCD.pl ..\lib\unicore\Name.pm            \
		   ..\lib\unicore\TestProp.pl

# Directories of Unicode data files generated by mktables
UNIDATADIR1	= ..\lib\unicore\To
UNIDATADIR2	= ..\lib\unicore\lib

PERLEXE_MANIFEST= .\perlexe.manifest
PERLEXE_ICO	= .\perlexe.ico
PERLEXE_RES	= .\perlexe.res
PERLDLL_RES	=

# Nominate a target which causes extensions to be re-built
# This used to be $(PERLEXE), but at worst it is the .dll that they depend
# on and really only the interface - i.e. the .def file used to export symbols
# from the .dll
PERLDEP		= perldll.def

PL2BAT		= bin\pl2bat.pl
GLOBBAT		= bin\perlglob.bat

UTILS		=			\
		..\utils\h2ph		\
		..\utils\splain		\
		..\utils\perlbug	\
		..\utils\pl2pm 		\
		..\utils\c2ph		\
		..\utils\pstruct	\
		..\utils\h2xs		\
		..\utils\perldoc	\
		..\utils\perlivp	\
		..\utils\libnetcfg	\
		..\utils\enc2xs		\
		..\utils\piconv		\
		..\utils\config_data	\
		..\utils\corelist	\
		..\utils\cpan		\
		..\utils\xsubpp		\
		..\utils\prove		\
		..\utils\ptar		\
		..\utils\ptardiff	\
		..\utils\ptargrep	\
		..\utils\zipdetails	\
		..\utils\shasum		\
		..\utils\instmodsh	\
		..\utils\json_pp	\
		..\utils\pod2html	\
		..\x2p\find2perl	\
		..\x2p\psed		\
		..\x2p\s2p		\
		bin\exetype.pl		\
		bin\runperl.pl		\
		bin\pl2bat.pl		\
		bin\perlglob.pl		\
		bin\search.pl

MAKE		= nmake -nologo
MAKE_BARE	= nmake

CFGSH_TMPL	= config.vc
CFGH_TMPL	= config_H.vc

XCOPY		= xcopy /f /r /i /d /y
RCOPY		= xcopy /f /r /i /e /d /y
NOOP		= @rem
NULL		=

DEL		= del

MICROCORE_SRC	=		\
		..\av.c		\
		..\caretx.c	\
		..\deb.c	\
		..\doio.c	\
		..\doop.c	\
		..\dump.c	\
		..\globals.c	\
		..\gv.c		\
		..\mro.c	\
		..\hv.c		\
		..\locale.c	\
		..\keywords.c	\
		..\mathoms.c    \
		..\mg.c		\
		..\numeric.c	\
		..\op.c		\
		..\pad.c	\
		..\perl.c	\
		..\perlapi.c	\
		..\perly.c	\
		..\pp.c		\
		..\pp_ctl.c	\
		..\pp_hot.c	\
		..\pp_pack.c	\
		..\pp_sort.c	\
		..\pp_sys.c	\
		..\reentr.c	\
		..\regcomp.c	\
		..\regexec.c	\
		..\run.c	\
		..\scope.c	\
		..\sv.c		\
		..\taint.c	\
		..\toke.c	\
		..\universal.c	\
		..\utf8.c	\
		..\util.c

EXTRACORE_SRC	= $(EXTRACORE_SRC) perllib.c

!IF "$(PERL_MALLOC)" == "define"
EXTRACORE_SRC	= $(EXTRACORE_SRC) ..\malloc.c
!ENDIF

EXTRACORE_SRC	= $(EXTRACORE_SRC) ..\perlio.c

WIN32_SRC	=		\
		.\win32.c	\
		.\win32sck.c	\
		.\win32thread.c	\
		.\fcrypt.c

# We need this for miniperl build unless we override canned 
# config.h #define building mini\*
#!IF "$(USE_PERLIO)" == "define"
WIN32_SRC	= $(WIN32_SRC) .\win32io.c
#!ENDIF


X2P_SRC		=		\
		..\x2p\a2p.c	\
		..\x2p\hash.c	\
		..\x2p\str.c	\
		..\x2p\util.c	\
		..\x2p\walk.c

CORE_NOCFG_H	=		\
		..\av.h		\
		..\cop.h	\
		..\cv.h		\
		..\dosish.h	\
		..\embed.h	\
		..\form.h	\
		..\gv.h		\
		..\handy.h	\
		..\hv.h		\
		..\hv_func.h	\
		..\iperlsys.h	\
		..\mg.h		\
		..\nostdio.h	\
		..\op.h		\
		..\opcode.h	\
		..\perl.h	\
		..\perlapi.h	\
		..\perlsdio.h	\
		..\perly.h	\
		..\pp.h		\
		..\proto.h	\
		..\regcomp.h	\
		..\regexp.h	\
		..\scope.h	\
		..\sv.h		\
		..\thread.h	\
		..\unixish.h	\
		..\utf8.h	\
		..\util.h	\
		..\warnings.h	\
		..\XSUB.h	\
		..\EXTERN.h	\
		..\perlvars.h	\
		..\intrpvar.h	\
		.\include\dirent.h	\
		.\include\netdb.h	\
		.\include\sys\errno2.h	\
		.\include\sys\socket.h	\
		.\win32.h

CORE_H		= $(CORE_NOCFG_H) .\config.h ..\git_version.h

UUDMAP_H	= ..\uudmap.h
BITCOUNT_H	= ..\bitcount.h
MG_DATA_H	= ..\mg_data.h
GENERATED_HEADERS = $(UUDMAP_H) $(BITCOUNT_H) $(MG_DATA_H)

MICROCORE_OBJ	= $(MICROCORE_SRC:.c=.obj)
CORE_OBJ	= $(MICROCORE_OBJ) $(EXTRACORE_SRC:.c=.obj)
WIN32_OBJ	= $(WIN32_SRC:.c=.obj)
MINICORE_OBJ	= $(MICROCORE_OBJ:..\=.\mini\)	\
		  $(MINIDIR)\miniperlmain$(o)	\
		  $(MINIDIR)\perlio$(o)
MINIWIN32_OBJ	= $(WIN32_OBJ:.\=.\mini\)
MINI_OBJ	= $(MINICORE_OBJ) $(MINIWIN32_OBJ)
DLL_OBJ		= $(DYNALOADER)
X2P_OBJ		= $(X2P_SRC:.c=.obj)
GENUUDMAP_OBJ	= $(GENUUDMAP:.exe=.obj)

PERLDLL_OBJ	= $(CORE_OBJ)
PERLEXE_OBJ	= perlmain$(o)
PERLEXEST_OBJ	= perlmainst$(o)

PERLDLL_OBJ	= $(PERLDLL_OBJ) $(WIN32_OBJ) $(DLL_OBJ)

!IF "$(USE_SETARGV)" != ""
SETARGV_OBJ	= setargv$(o)
!ENDIF

!IF "$(ALL_STATIC)" == "define"
# some exclusions, unfortunately, until fixed:
#  - MakeMaker isn't capable enough for SDBM_File (small bug)
STATIC_EXT	= * !SDBM_File
!ELSE
# specify static extensions here, for example:
# (be sure to include Win32CORE to load Win32 on demand)
#STATIC_EXT	= Win32CORE Cwd Compress/Raw/Zlib
STATIC_EXT	= Win32CORE
!ENDIF

DYNALOADER	= ..\DynaLoader$(o)

CFG_VARS	=					\
		"INST_TOP=$(INST_TOP)"			\
		"INST_VER=$(INST_VER)"			\
		"INST_ARCH=$(INST_ARCH)"		\
		"archname=$(ARCHNAME)"			\
		"cc=$(CC)"				\
		"ld=$(LINK32)"				\
		"ccflags=$(EXTRACFLAGS) $(OPTIMIZE:"=\") $(DEFINES) $(BUILDOPT)"	\
		"cf_email=$(EMAIL)"	 		\
		"d_mymalloc=$(PERL_MALLOC)"		\
		"libs=$(LIBFILES)"			\
		"incpath=$(CCINCDIR:"=\")"		\
		"libperl=$(PERLIMPLIB:..\=)"		\
		"libpth=$(CCLIBDIR:"=\");$(EXTRALIBDIRS:"=\")"	\
		"libc=$(LIBC)"				\
		"make=$(MAKE_BARE)"				\
		"static_ext=$(STATIC_EXT)"		\
		"usethreads=$(USE_ITHREADS)"		\
		"useithreads=$(USE_ITHREADS)"		\
		"usemultiplicity=$(USE_MULTI)"		\
		"useperlio=$(USE_PERLIO)"		\
		"use64bitint=$(USE_64_BIT_INT)"		\
		"uselargefiles=$(USE_LARGE_FILES)"	\
		"usesitecustomize=$(USE_SITECUST)"	\
		"LINK_FLAGS=$(LINK_FLAGS:"=\")"		\
		"optimize=$(OPTIMIZE:"=\")"		\
		"WIN64=$(WIN64)"

#
# Top targets
#

all : .\config.h ..\git_version.h $(GLOBEXE) $(CONFIGPM) \
	$(UNIDATAFILES) MakePPPort $(PERLEXE) $(X2P) Extensions_nonxs Extensions $(PERLSTATIC)
	@echo	Everything is up to date. '$(MAKE_BARE) test' to run test suite.

regnodes : ..\regnodes.h

..\regcomp$(o) : ..\regnodes.h ..\regcharclass.h

..\regexec$(o) : ..\regnodes.h ..\regcharclass.h

reonly : regnodes .\config.h ..\git_version.h $(GLOBEXE) $(CONFIGPM) \
	$(UNIDATAFILES) $(PERLEXE) $(X2P) Extensions_reonly
	@echo	Perl and 're' are up to date.

static: $(PERLEXESTATIC)

#------------------------------------------------------------

$(GLOBEXE) : perlglob$(o)
	$(LINK32) $(LINK_FLAGS) $(LIBFILES) -out:$@ -subsystem:$(SUBSYS) \
	    perlglob$(o) setargv$(o)
	$(EMBED_EXE_MANI)

perlglob$(o)  : perlglob.c

config.w32 : $(CFGSH_TMPL)
	copy $(CFGSH_TMPL) config.w32

#
# Copy the template config.h and set configurables at the end of it
# as per the options chosen and compiler used.
# Note: This config.h is only used to build miniperl.exe anyway, but
# it's as well to have its options correct to be sure that it builds
# and so that it's "-V" options are correct for use by makedef.pl. The
# real config.h used to build perl.exe is generated from the top-level
# config_h.SH by config_h.PL (run by miniperl.exe).
#
.\config.h : $(CFGH_TMPL)
	-del /f config.h
	copy $(CFGH_TMPL) config.h
	@echo.>>$@
	@echo #ifndef _config_h_footer_>>$@
	@echo #define _config_h_footer_>>$@
	@echo #undef Off_t>>$@
	@echo #undef LSEEKSIZE>>$@
	@echo #undef Off_t_size>>$@
	@echo #undef PTRSIZE>>$@
	@echo #undef SSize_t>>$@
	@echo #undef HAS_ATOLL>>$@
	@echo #undef HAS_STRTOLL>>$@
	@echo #undef HAS_STRTOULL>>$@
	@echo #undef IVTYPE>>$@
	@echo #undef UVTYPE>>$@
	@echo #undef IVSIZE>>$@
	@echo #undef UVSIZE>>$@
	@echo #undef NV_PRESERVES_UV>>$@
	@echo #undef NV_PRESERVES_UV_BITS>>$@
	@echo #undef IVdf>>$@
	@echo #undef UVuf>>$@
	@echo #undef UVof>>$@
	@echo #undef UVxf>>$@
	@echo #undef UVXf>>$@
	@echo #undef USE_64_BIT_INT>>$@
	@echo #undef Size_t_size>>$@
!IF "$(USE_LARGE_FILES)"=="define"
	@echo #define Off_t __int64>>$@
	@echo #define LSEEKSIZE ^8>>$@
	@echo #define Off_t_size ^8>>$@
!ELSE
	@echo #define Off_t long>>$@
	@echo #define LSEEKSIZE ^4>>$@
	@echo #define Off_t_size ^4>>$@
!ENDIF
!IF "$(WIN64)"=="define"
	@echo #define PTRSIZE ^8>>$@
	@echo #define SSize_t __int64>>$@
	@echo #define HAS_ATOLL>>$@
	@echo #define HAS_STRTOLL>>$@
	@echo #define HAS_STRTOULL>>$@
	@echo #define Size_t_size ^8>>$@
!ELSE
	@echo #define PTRSIZE ^4>>$@
	@echo #define SSize_t int>>$@
	@echo #undef HAS_ATOLL>>$@
	@echo #undef HAS_STRTOLL>>$@
	@echo #undef HAS_STRTOULL>>$@
	@echo #define Size_t_size ^4>>$@
!ENDIF
!IF "$(USE_64_BIT_INT)"=="define"
	@echo #define IVTYPE __int64>>$@
	@echo #define UVTYPE unsigned __int64>>$@
	@echo #define IVSIZE ^8>>$@
	@echo #define UVSIZE ^8>>$@
	@echo #undef NV_PRESERVES_UV>>$@
	@echo #define NV_PRESERVES_UV_BITS 53>>$@
	@echo #define IVdf "I64d">>$@
	@echo #define UVuf "I64u">>$@
	@echo #define UVof "I64o">>$@
	@echo #define UVxf "I64x">>$@
	@echo #define UVXf "I64X">>$@
	@echo #define USE_64_BIT_INT>>$@
!ELSE
	@echo #define IVTYPE long>>$@
	@echo #define UVTYPE unsigned long>>$@
	@echo #define IVSIZE ^4>>$@
	@echo #define UVSIZE ^4>>$@
	@echo #define NV_PRESERVES_UV>>$@
	@echo #define NV_PRESERVES_UV_BITS 32>>$@
	@echo #define IVdf "ld">>$@
	@echo #define UVuf "lu">>$@
	@echo #define UVof "lo">>$@
	@echo #define UVxf "lx">>$@
	@echo #define UVXf "lX">>$@
	@echo #undef USE_64_BIT_INT>>$@
!ENDIF
	@echo #endif>>$@

..\git_version.h : $(MINIPERL) ..\make_patchnum.pl
	cd .. && miniperl -Ilib make_patchnum.pl && cd win32

# make sure that we recompile perl.c if the git version changes
..\perl$(o) : ..\git_version.h

..\config.sh : config.w32 $(MINIPERL) config_sh.PL FindExt.pm
	$(MINIPERL) -I..\lib config_sh.PL $(CFG_VARS) config.w32 > ..\config.sh

# This target is for when changes to the main config.sh happen.
# Edit config.vc, then make perl in a minimal configuration (i.e. with MULTI,
# ITHREADS, IMP_SYS, LARGE_FILES and PERLIO off), then make this target
# to regenerate config_H.vc.
regen_config_h:
	$(MINIPERL) -I..\lib config_sh.PL $(CFG_VARS) $(CFGSH_TMPL) > ..\config.sh
	$(MINIPERL) -I..\lib ..\configpm --chdir=..
	-del /f $(CFGH_TMPL)
	-$(MINIPERL) -I..\lib config_h.PL
	rename config.h $(CFGH_TMPL)

$(CONFIGPM) : $(MINIPERL) ..\config.sh config_h.PL
	$(MINIPERL) -I..\lib ..\configpm --chdir=..
	if exist lib\* $(RCOPY) lib\*.* ..\lib\$(NULL)
	$(XCOPY) ..\*.h $(COREDIR)\*.*
	$(XCOPY) *.h $(COREDIR)\*.*
	$(RCOPY) include $(COREDIR)\*.*
	-$(MINIPERL) -I..\lib config_h.PL
	if errorlevel 1 $(MAKE) /$(MAKEFLAGS) $(CONFIGPM)

# See the comment in Makefile.SH explaining this seemingly cranky ordering
$(MINIPERL) : ..\lib\buildcustomize.pl 

..\lib\buildcustomize.pl : $(MINIDIR) $(MINI_OBJ) ..\write_buildcustomize.pl
	$(LINK32) -subsystem:console -out:$(MINIPERL) @<<
	$(LINK_FLAGS) $(DELAYLOAD) $(LIBFILES) $(MINI_OBJ)
<<
	$(EMBED_EXE_MANI:..\lib\buildcustomize.pl=..\miniperl.exe)
	$(MINIPERL) -I..\lib -f ..\write_buildcustomize.pl ..

$(MINIDIR) :
	if not exist "$(MINIDIR)" mkdir "$(MINIDIR)"

$(MINICORE_OBJ) : $(CORE_NOCFG_H)
	$(CC) -c $(CFLAGS) -DPERL_EXTERNAL_GLOB -DPERL_IS_MINIPERL $(OBJOUT_FLAG)$@ ..\$(*F).c

$(MINIWIN32_OBJ) : $(CORE_NOCFG_H)
	$(CC) -c $(CFLAGS) -DPERL_IS_MINIPERL $(OBJOUT_FLAG)$@ $(*F).c

# -DPERL_IMPLICIT_SYS needs C++ for perllib.c
# This is the only file that depends on perlhost.h, vmem.h, and vdir.h
!IF "$(USE_IMP_SYS)" == "define"
perllib$(o)	: perllib.c .\perlhost.h .\vdir.h .\vmem.h
	$(CC) -c -I. $(CFLAGS_O) $(CXX_FLAG) $(OBJOUT_FLAG)$@ perllib.c
!ENDIF

# 1. we don't want to rebuild miniperl.exe when config.h changes
# 2. we don't want to rebuild miniperl.exe with non-default config.h
# 3. we can't have miniperl.exe depend on git_version.h, as miniperl creates it
$(MINI_OBJ)	: $(CORE_NOCFG_H)

$(WIN32_OBJ)	: $(CORE_H)
$(CORE_OBJ)	: $(CORE_H)
$(DLL_OBJ)	: $(CORE_H)
$(X2P_OBJ)	: $(CORE_H)

perldll.def : $(MINIPERL) $(CONFIGPM) ..\embed.fnc ..\makedef.pl create_perllibst_h.pl
	$(MINIPERL) -I..\lib create_perllibst_h.pl
	$(MINIPERL) -I..\lib -w ..\makedef.pl PLATFORM=win32 $(OPTIMIZE) $(DEFINES) $(BUILDOPT) \
	    CCTYPE=$(CCTYPE) TARG_DIR=..\ > perldll.def

$(PERLDLL): perldll.def $(PERLDLL_OBJ) $(PERLDLL_RES) Extensions_static
	$(LINK32) -dll -def:perldll.def -base:0x28000000 -out:$@ @Extensions_static @<<
		$(LINK_FLAGS) $(DELAYLOAD) $(LIBFILES) $(PERLDLL_OBJ) $(PERLDLL_RES)
<<
	$(EMBED_DLL_MANI)
	$(XCOPY) $(PERLIMPLIB) $(COREDIR)

$(PERLSTATICLIB): Extensions_static
	$(LIB32) $(LIB_FLAGS) -out:$@ @Extensions_static @<<
		$(PERLDLL_OBJ)
<<
	$(XCOPY) $(PERLSTATICLIB) $(COREDIR)

$(PERLEXE_RES): perlexe.rc $(PERLEXE_MANIFEST) $(PERLEXE_ICO)

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

$(X2P) : $(MINIPERL) $(X2P_OBJ) Extensions
	$(MINIPERL) -I..\lib ..\x2p\find2perl.PL
	$(MINIPERL) -I..\lib ..\x2p\s2p.PL
	$(LINK32) -subsystem:console -out:$@ @<<
		$(LINK_FLAGS) $(LIBFILES) $(X2P_OBJ)
<<
	$(EMBED_EXE_MANI)

$(MINIDIR)\globals$(o) : $(GENERATED_HEADERS)

$(UUDMAP_H) $(MG_DATA_H) : $(BITCOUNT_H)

$(BITCOUNT_H) : $(GENUUDMAP)
	$(GENUUDMAP) $(GENERATED_HEADERS)

$(GENUUDMAP_OBJ) : ..\mg_raw.h

$(GENUUDMAP) : $(GENUUDMAP_OBJ)
	$(LINK32) -subsystem:console -out:$@ @<<
		$(LINK_FLAGS) $(LIBFILES) $(GENUUDMAP_OBJ)
<<
	$(EMBED_EXE_MANI)

perlmain.c : runperl.c
	copy runperl.c perlmain.c

perlmain$(o) : perlmain.c
	$(CC) $(CFLAGS_O:-DPERLDLL=-UPERLDLL) $(OBJOUT_FLAG)$@ -c perlmain.c

perlmainst.c : runperl.c
	copy runperl.c perlmainst.c

perlmainst$(o) : perlmainst.c
	$(CC) $(CFLAGS_O) $(OBJOUT_FLAG)$@ -c perlmainst.c

$(PERLEXE): $(PERLDLL) $(CONFIGPM) $(PERLEXE_OBJ) $(PERLEXE_RES)
	$(LINK32) -subsystem:console -out:$@ $(LINK_FLAGS) \
	    $(PERLEXE_OBJ) $(PERLEXE_RES) $(PERLIMPLIB) $(LIBFILES) $(SETARGV_OBJ)
	$(EMBED_EXE_MANI)
	copy $(PERLEXE) $(WPERLEXE)
	$(MINIPERL) -I..\lib bin\exetype.pl $(WPERLEXE) WINDOWS

$(PERLEXESTATIC): $(PERLSTATICLIB) $(CONFIGPM) $(PERLEXEST_OBJ) $(PERLEXE_RES)
	$(LINK32) -subsystem:console -out:$@ $(LINK_FLAGS) \
	    $(PERLEXEST_OBJ) $(PERLEXE_RES) $(PERLSTATICLIB) $(LIBFILES) $(SETARGV_OBJ)
	$(EMBED_EXE_MANI)

MakePPPort: $(MINIPERL) $(CONFIGPM) Extensions_nonxs
	$(MINIPERL) -I..\lib ..\mkppport

#-------------------------------------------------------------------------------
# There's no direct way to mark a dependency on
# DynaLoader.pm, so this will have to do
Extensions: ..\make_ext.pl ..\lib\buildcustomize.pl $(PERLDEP) $(CONFIGPM) $(DYNALOADER)
	$(XCOPY) ..\*.h $(COREDIR)\*.*
	$(MINIPERL) -I..\lib ..\make_ext.pl "MAKE=$(MAKE)" --dir=$(CPANDIR) --dir=$(DISTDIR) --dir=$(EXTDIR) --dynamic

Extensions_reonly: ..\make_ext.pl ..\lib\buildcustomize.pl $(PERLDEP) $(CONFIGPM) $(DYNALOADER)
	$(XCOPY) ..\*.h $(COREDIR)\*.*
	$(MINIPERL) -I..\lib ..\make_ext.pl "MAKE=$(MAKE)" --dir=$(CPANDIR) --dir=$(DISTDIR) --dir=$(EXTDIR) --dynamic +re

Extensions_static : ..\make_ext.pl ..\lib\buildcustomize.pl list_static_libs.pl $(PERLDEP) $(CONFIGPM) Extensions_nonxs
	$(XCOPY) ..\*.h $(COREDIR)\*.*
	$(MINIPERL) -I..\lib ..\make_ext.pl "MAKE=$(MAKE)" --dir=$(CPANDIR) --dir=$(DISTDIR) --dir=$(EXTDIR) --static
	$(MINIPERL) -I..\lib list_static_libs.pl > Extensions_static

Extensions_nonxs: ..\make_ext.pl ..\lib\buildcustomize.pl $(PERLDEP) $(CONFIGPM) ..\pod\perlfunc.pod
	$(XCOPY) ..\*.h $(COREDIR)\*.*
	$(MINIPERL) -I..\lib ..\make_ext.pl "MAKE=$(MAKE)" --dir=$(CPANDIR) --dir=$(DISTDIR) --dir=$(EXTDIR) --nonxs

$(DYNALOADER) : ..\make_ext.pl ..\lib\buildcustomize.pl $(PERLDEP) $(CONFIGPM) Extensions_nonxs
	$(XCOPY) ..\*.h $(COREDIR)\*.*
	$(MINIPERL) -I..\lib ..\make_ext.pl "MAKE=$(MAKE)" --dir=$(EXTDIR) --dynaloader

Extensions_clean: 
	-if exist $(MINIPERL) $(MINIPERL) -I..\lib ..\make_ext.pl "MAKE=$(MAKE)" --dir=$(CPANDIR) --dir=$(DISTDIR) --dir=$(EXTDIR) --all --target=clean

Extensions_realclean: 
	-if exist $(MINIPERL) $(MINIPERL) -I..\lib ..\make_ext.pl "MAKE=$(MAKE)" --dir=$(CPANDIR) --dir=$(DISTDIR) --dir=$(EXTDIR) --all --target=realclean

#-------------------------------------------------------------------------------

doc: $(PERLEXE) ..\pod\perltoc.pod
	$(PERLEXE) -I..\lib ..\installhtml --podroot=.. --htmldir=$(HTMLDIR) \
	    --podpath=pod:lib:utils --htmlroot="file://$(INST_HTML::=|)" \
	    --recurse

..\utils\Makefile: $(CONFIGPM) ..\utils\Makefile.PL
	$(MINIPERL) -I..\lib ..\utils\Makefile.PL ..

# Note that this next section is parsed (and regenerated) by pod/buildtoc
# so please check that script before making structural changes here

utils: $(PERLEXE) $(X2P) ..\utils\Makefile
	cd ..\utils
	$(MAKE) PERL=$(MINIPERL)
	cd ..\pod
	copy ..\README.aix      ..\pod\perlaix.pod
	copy ..\README.amiga    ..\pod\perlamiga.pod
	copy ..\README.android  ..\pod\perlandroid.pod
	copy ..\README.bs2000   ..\pod\perlbs2000.pod
	copy ..\README.ce       ..\pod\perlce.pod
	copy ..\README.cn       ..\pod\perlcn.pod
	copy ..\README.cygwin   ..\pod\perlcygwin.pod
	copy ..\README.dos      ..\pod\perldos.pod
	copy ..\README.freebsd  ..\pod\perlfreebsd.pod
	copy ..\README.haiku    ..\pod\perlhaiku.pod
	copy ..\README.hpux     ..\pod\perlhpux.pod
	copy ..\README.hurd     ..\pod\perlhurd.pod
	copy ..\README.irix     ..\pod\perlirix.pod
	copy ..\README.jp       ..\pod\perljp.pod
	copy ..\README.ko       ..\pod\perlko.pod
	copy ..\README.linux    ..\pod\perllinux.pod
	copy ..\README.macos    ..\pod\perlmacos.pod
	copy ..\README.macosx   ..\pod\perlmacosx.pod
	copy ..\README.netware  ..\pod\perlnetware.pod
	copy ..\README.openbsd  ..\pod\perlopenbsd.pod
	copy ..\README.os2      ..\pod\perlos2.pod
	copy ..\README.os390    ..\pod\perlos390.pod
	copy ..\README.os400    ..\pod\perlos400.pod
	copy ..\README.plan9    ..\pod\perlplan9.pod
	copy ..\README.qnx      ..\pod\perlqnx.pod
	copy ..\README.riscos   ..\pod\perlriscos.pod
	copy ..\README.solaris  ..\pod\perlsolaris.pod
	copy ..\README.symbian  ..\pod\perlsymbian.pod
	copy ..\README.synology ..\pod\perlsynology.pod
	copy ..\README.tru64    ..\pod\perltru64.pod
	copy ..\README.tw       ..\pod\perltw.pod
	copy ..\README.vos      ..\pod\perlvos.pod
	copy ..\README.win32    ..\pod\perlwin32.pod
	copy ..\pod\perldelta.pod ..\pod\perl5201delta.pod
	cd ..\win32
	$(PERLEXE) $(PL2BAT) $(UTILS)
	$(MINIPERL) -I..\lib ..\autodoc.pl ..
	$(MINIPERL) -I..\lib ..\pod\perlmodlib.PL -q ..

..\pod\perltoc.pod: $(PERLEXE) Extensions Extensions_nonxs
	$(PERLEXE) -f ..\pod\buildtoc -q

# Note that the pod cleanup in this next section is parsed (and regenerated
# by pod/buildtoc so please check that script before making changes here

distclean: realclean
	-del /f $(MINIPERL) $(PERLEXE) $(PERLDLL) $(GLOBEXE) \
		$(PERLIMPLIB) ..\miniperl.lib $(PERLEXESTATIC) $(PERLSTATICLIB)
	-del /f $(LIBDIR)\Encode.pm $(LIBDIR)\encoding.pm $(LIBDIR)\Errno.pm
	-del /f $(LIBDIR)\Config.pod $(LIBDIR)\POSIX.pod $(LIBDIR)\threads.pm
	-del /f $(LIBDIR)\.exists $(LIBDIR)\attributes.pm $(LIBDIR)\DynaLoader.pm
	-del /f $(LIBDIR)\Fcntl.pm $(LIBDIR)\IO.pm $(LIBDIR)\Opcode.pm
	-del /f $(LIBDIR)\ops.pm $(LIBDIR)\Safe.pm
	-del /f $(LIBDIR)\SDBM_File.pm $(LIBDIR)\Socket.pm $(LIBDIR)\POSIX.pm
	-del /f $(LIBDIR)\B.pm $(LIBDIR)\O.pm $(LIBDIR)\re.pm
	-del /f $(LIBDIR)\File\Glob.pm
	-del /f $(LIBDIR)\Storable.pm
	-del /f $(LIBDIR)\Sys\Hostname.pm
	-del /f $(LIBDIR)\Time\HiRes.pm
	-del /f $(LIBDIR)\Unicode\Normalize.pm
	-del /f $(LIBDIR)\Math\BigInt\FastCalc.pm
	-del /f $(LIBDIR)\Win32.pm
	-del /f $(LIBDIR)\Win32CORE.pm
	-del /f $(LIBDIR)\Win32API\File.pm
	-del /f $(LIBDIR)\Win32API\File\cFile.pc
	-del /f $(LIBDIR)\buildcustomize.pl
	-del /f $(DISTDIR)\XSLoader\XSLoader.pm
	-del /f *.def *.map
	-if exist $(LIBDIR)\App rmdir /s /q $(LIBDIR)\App
	-if exist $(LIBDIR)\Archive rmdir /s /q $(LIBDIR)\Archive
	-if exist $(LIBDIR)\Attribute rmdir /s /q $(LIBDIR)\Attribute
	-if exist $(LIBDIR)\autodie rmdir /s /q $(LIBDIR)\autodie
	-if exist $(LIBDIR)\Carp rmdir /s /q $(LIBDIR)\Carp
	-if exist $(LIBDIR)\CGI rmdir /s /q $(LIBDIR)\CGI
	-if exist $(LIBDIR)\Compress rmdir /s /q $(LIBDIR)\Compress
	-if exist $(LIBDIR)\Config\Perl rmdir /s /q $(LIBDIR)\Config\Perl
	-if exist $(LIBDIR)\CPAN rmdir /s /q $(LIBDIR)\CPAN
	-if exist $(LIBDIR)\Data rmdir /s /q $(LIBDIR)\Data
	-if exist $(LIBDIR)\Devel rmdir /s /q $(LIBDIR)\Devel
	-if exist $(LIBDIR)\Digest rmdir /s /q $(LIBDIR)\Digest
	-if exist $(LIBDIR)\Encode rmdir /s /q $(LIBDIR)\Encode
	-if exist $(LIBDIR)\encoding rmdir /s /q $(LIBDIR)\encoding
	-if exist $(LIBDIR)\Exporter rmdir /s /q $(LIBDIR)\Exporter
	-if exist $(LIBDIR)\ExtUtils\CBuilder rmdir /s /q $(LIBDIR)\ExtUtils\CBuilder
	-if exist $(LIBDIR)\ExtUtils\Command rmdir /s /q $(LIBDIR)\ExtUtils\Command
	-if exist $(LIBDIR)\ExtUtils\Constant rmdir /s /q $(LIBDIR)\ExtUtils\Constant
	-if exist $(LIBDIR)\ExtUtils\Liblist rmdir /s /q $(LIBDIR)\ExtUtils\Liblist
	-if exist $(LIBDIR)\ExtUtils\MakeMaker rmdir /s /q $(LIBDIR)\ExtUtils\MakeMaker
	-if exist $(LIBDIR)\ExtUtils\ParseXS rmdir /s /q $(LIBDIR)\ExtUtils\ParseXS
	-if exist $(LIBDIR)\ExtUtils\Typemaps rmdir /s /q $(LIBDIR)\ExtUtils\Typemaps
	-if exist $(LIBDIR)\File\Spec rmdir /s /q $(LIBDIR)\File\Spec
	-if exist $(LIBDIR)\Filter rmdir /s /q $(LIBDIR)\Filter
	-if exist $(LIBDIR)\Hash rmdir /s /q $(LIBDIR)\Hash
	-if exist $(LIBDIR)\HTTP rmdir /s /q $(LIBDIR)\HTTP
	-if exist $(LIBDIR)\I18N rmdir /s /q $(LIBDIR)\I18N
	-if exist $(LIBDIR)\inc rmdir /s /q $(LIBDIR)\inc
	-if exist $(LIBDIR)\IO rmdir /s /q $(LIBDIR)\IO
	-if exist $(LIBDIR)\IPC rmdir /s /q $(LIBDIR)\IPC
	-if exist $(LIBDIR)\JSON rmdir /s /q $(LIBDIR)\JSON
	-if exist $(LIBDIR)\List rmdir /s /q $(LIBDIR)\List
	-if exist $(LIBDIR)\Locale rmdir /s /q $(LIBDIR)\Locale
	-if exist $(LIBDIR)\Math rmdir /s /q $(LIBDIR)\Math
	-if exist $(LIBDIR)\Memoize rmdir /s /q $(LIBDIR)\Memoize
	-if exist $(LIBDIR)\MIME rmdir /s /q $(LIBDIR)\MIME
	-if exist $(LIBDIR)\Module rmdir /s /q $(LIBDIR)\Module
	-if exist $(LIBDIR)\Net\FTP rmdir /s /q $(LIBDIR)\Net\FTP
	-if exist $(LIBDIR)\OpenBSD rmdir /s /q $(LIBDIR)\OpenBSD
	-if exist $(LIBDIR)\Package rmdir /s /q $(LIBDIR)\Package
	-if exist $(LIBDIR)\Params rmdir /s /q $(LIBDIR)\Params
	-if exist $(LIBDIR)\Parse rmdir /s /q $(LIBDIR)\Parse
	-if exist $(LIBDIR)\Perl rmdir /s /q $(LIBDIR)\Perl
	-if exist $(LIBDIR)\PerlIO rmdir /s /q $(LIBDIR)\PerlIO
	-if exist $(LIBDIR)\Pod\Perldoc rmdir /s /q $(LIBDIR)\Pod\Perldoc
	-if exist $(LIBDIR)\Pod\Simple rmdir /s /q $(LIBDIR)\Pod\Simple
	-if exist $(LIBDIR)\Pod\Text rmdir /s /q $(LIBDIR)\Pod\Text
	-if exist $(LIBDIR)\Scalar rmdir /s /q $(LIBDIR)\Scalar
	-if exist $(LIBDIR)\Search rmdir /s /q $(LIBDIR)\Search
	-if exist $(LIBDIR)\Sys rmdir /s /q $(LIBDIR)\Sys
	-if exist $(LIBDIR)\TAP rmdir /s /q $(LIBDIR)\TAP
	-if exist $(LIBDIR)\Term rmdir /s /q $(LIBDIR)\Term
	-if exist $(LIBDIR)\Test rmdir /s /q $(LIBDIR)\Test
	-if exist $(LIBDIR)\Text rmdir /s /q $(LIBDIR)\Text
	-if exist $(LIBDIR)\Thread rmdir /s /q $(LIBDIR)\Thread
	-if exist $(LIBDIR)\threads rmdir /s /q $(LIBDIR)\threads
	-if exist $(LIBDIR)\Tie\Hash rmdir /s /q $(LIBDIR)\Tie\Hash
	-if exist $(LIBDIR)\Unicode\Collate rmdir /s /q $(LIBDIR)\Unicode\Collate
	-if exist $(LIBDIR)\Unicode\Collate\Locale rmdir /s /q $(LIBDIR)\Unicode\Collate\Locale
	-if exist $(LIBDIR)\version rmdir /s /q $(LIBDIR)\version
	-if exist $(LIBDIR)\VMS rmdir /s /q $(LIBDIR)\VMS
	-if exist $(LIBDIR)\Win32API rmdir /s /q $(LIBDIR)\Win32API
	-if exist $(LIBDIR)\XS rmdir /s /q $(LIBDIR)\XS
	-cd $(PODDIR) && del /f *.html *.bat roffitall \
	    perl5201delta.pod perlaix.pod perlamiga.pod perlandroid.pod \
	    perlapi.pod perlbs2000.pod perlce.pod perlcn.pod perlcygwin.pod \
	    perldos.pod perlfreebsd.pod perlhaiku.pod perlhpux.pod \
	    perlhurd.pod perlintern.pod perlirix.pod perljp.pod perlko.pod \
	    perllinux.pod perlmacos.pod perlmacosx.pod perlmodlib.pod \
	    perlnetware.pod perlopenbsd.pod perlos2.pod perlos390.pod \
	    perlos400.pod perlplan9.pod perlqnx.pod perlriscos.pod \
	    perlsolaris.pod perlsymbian.pod perlsynology.pod perltoc.pod \
	    perltru64.pod perltw.pod perlvos.pod perlwin32.pod
	-cd ..\utils && del /f h2ph splain perlbug pl2pm c2ph pstruct h2xs \
	    perldoc perlivp libnetcfg enc2xs piconv cpan *.bat \
	    xsubpp pod2html instmodsh json_pp prove ptar ptardiff ptargrep shasum corelist config_data zipdetails
	-cd ..\x2p && del /f find2perl s2p psed *.bat
	-del /f ..\config.sh perlmain.c dlutils.c config.h.new \
	        perlmainst.c
	-del /f $(CONFIGPM)
	-del /f ..\lib\Config_git.pl
	-del /f bin\*.bat
	-del /f perllibst.h
	-del /f $(PERLEXE_RES) perl.base
	-cd .. && del /s *.lib *.map *.pdb *.ilk *.bs *$(o) .exists pm_to_blib ppport.h
	-cd $(EXTDIR) && del /s *.def Makefile Makefile.old
	-cd $(DISTDIR) && del /s *.def Makefile Makefile.old
	-cd $(CPANDIR) && del /s *.def Makefile Makefile.old
	-del /s ..\utils\Makefile
	-if exist $(AUTODIR) rmdir /s /q $(AUTODIR)
	-if exist $(COREDIR) rmdir /s /q $(COREDIR)
	-if exist pod2htmd.tmp del pod2htmd.tmp
	-if exist $(HTMLDIR) rmdir /s /q $(HTMLDIR)
	-del /f ..\t\test_state

install : all installbare installhtml

installbare : utils ..\pod\perltoc.pod
	$(PERLEXE) ..\installperl
	if exist $(WPERLEXE) $(XCOPY) $(WPERLEXE) $(INST_BIN)\*.*
	if exist $(PERLEXESTATIC) $(XCOPY) $(PERLEXESTATIC) $(INST_BIN)\*.*
	$(XCOPY) $(GLOBEXE) $(INST_BIN)\*.*
	if exist ..\perl*.pdb $(XCOPY) ..\perl*.pdb $(INST_BIN)\*.*
	if exist ..\x2p\a2p.pdb $(XCOPY) ..\x2p\a2p.pdb $(INST_BIN)\*.*
	$(XCOPY) bin\*.bat $(INST_SCRIPT)\*.*

installhtml : doc
	$(RCOPY) $(HTMLDIR)\*.* $(INST_HTML)\*.*

inst_lib : $(CONFIGPM)
	$(RCOPY) ..\lib $(INST_LIB)\*.*

$(UNIDATAFILES) ..\pod\perluniprops.pod : $(MINIPERL) $(CONFIGPM) ..\lib\unicore\mktables Extensions_nonxs
	cd ..\lib\unicore && \
	..\$(MINIPERL) -I.. -I..\..\dist\Cwd\lib -I..\..\dist\Cwd mktables -P ..\..\pod -maketest -makelist -p -check $@ $(FIRSTUNIFILE)

minitest : $(MINIPERL) $(GLOBEXE) $(CONFIGPM) utils $(UNIDATAFILES)
	$(XCOPY) $(MINIPERL) ..\t\$(NULL)
	if exist ..\t\perl.exe del /f ..\t\perl.exe
	rename ..\t\miniperl.exe perl.exe
	$(XCOPY) $(GLOBEXE) ..\t\$(NULL)
	attrib -r ..\t\*.*
	cd ..\t && \
	$(MINIPERL) -I..\lib harness base/*.t comp/*.t cmd/*.t io/*.t opbasic/*.t op/*.t pragma/*.t

test-prep : all utils ../pod/perltoc.pod
	$(XCOPY) $(PERLEXE) ..\t\$(NULL)
	$(XCOPY) $(PERLDLL) ..\t\$(NULL)
	$(XCOPY) $(GLOBEXE) ..\t\$(NULL)
	set PERL_STATIC_EXT=$(STATIC_EXT)

test : test-prep
	cd ..\t
	$(PERLEXE) -I..\lib harness $(TEST_SWITCHES) $(TEST_FILES)
	cd ..\win32

test_porting : test-prep
	cd ..\t
	$(PERLEXE) -I..\lib harness $(TEST_SWITCHES) porting\*.t ..\lib\diagnostics.t
	cd ..\win32

test-reonly : reonly utils
	$(XCOPY) $(PERLEXE) ..\t\$(NULL)
	$(XCOPY) $(PERLDLL) ..\t\$(NULL)
	$(XCOPY) $(GLOBEXE) ..\t\$(NULL)
	cd ..\t
	$(PERLEXE) -I..\lib harness $(OPT) -re \bre\\/ $(EXTRA)
	cd ..\win32

regen :
	cd ..
	regen.pl
	cd win32

test-notty : test-prep
	set PERL_SKIP_TTY_TEST=1
	cd ..\t
	$(PERLEXE) -I..\lib harness $(TEST_SWITCHES) $(TEST_FILES)
	cd ..\win32

_test : 
       $(XCOPY) $(PERLEXE) ..\t\$(NULL)
       $(XCOPY) $(PERLDLL) ..\t\$(NULL)
       $(XCOPY) $(GLOBEXE) ..\t\$(NULL)
       cd ..\t
       $(PERLEXE) -I..\lib harness $(TEST_SWITCHES) $(TEST_FILES)
       cd ..\win32

_clean :
	-@$(DEL) miniperlmain$(o)
	-@$(DEL) $(MINIPERL)
	-@$(DEL) perlglob$(o)
	-@$(DEL) perlmain$(o)
	-@$(DEL) perlmainst$(o)
	-@$(DEL) config.w32
	-@$(DEL) config.h
	-@$(DEL) ..\git_version.h
	-@$(DEL) $(GLOBEXE)
	-@$(DEL) $(PERLEXE)
	-@$(DEL) $(WPERLEXE)
	-@$(DEL) $(PERLEXESTATIC)
	-@$(DEL) $(PERLSTATICLIB)
	-@$(DEL) $(PERLDLL)
	-@$(DEL) $(CORE_OBJ)
	-@$(DEL) $(GENUUDMAP) $(GENUUDMAP_OBJ) $(GENERATED_HEADERS)
	-if exist $(MINIDIR) rmdir /s /q $(MINIDIR)
	-if exist $(UNIDATADIR1) rmdir /s /q $(UNIDATADIR1)
	-if exist $(UNIDATADIR2) rmdir /s /q $(UNIDATADIR2)
	-@$(DEL) $(UNIDATAFILES)
	-@$(DEL) $(WIN32_OBJ)
	-@$(DEL) $(DLL_OBJ)
	-@$(DEL) $(X2P_OBJ)
	-@$(DEL) ..\*$(o) ..\*.lib ..\*.exp *$(o) *.lib *.exp *.res
	-@$(DEL) ..\t\*.exe ..\t\*.dll ..\t\*.bat
	-@$(DEL) ..\x2p\*.exe ..\x2p\*.bat
	-@$(DEL) *.ilk
	-@$(DEL) *.pdb
	-@$(DEL) Extensions_static

clean : Extensions_clean _clean

realclean : Extensions_realclean _clean

# Handy way to run perlbug -ok without having to install and run the
# installed perlbug. We don't re-run the tests here - we trust the user.
# Please *don't* use this unless all tests pass.
# If you want to report test failures, use "nmake nok" instead.
ok: utils
	$(PERLEXE) -I..\lib ..\utils\perlbug -ok -s "(UNINSTALLED)"

okfile: utils
	$(PERLEXE) -I..\lib ..\utils\perlbug -ok -s "(UNINSTALLED)" -F perl.ok

nok: utils
	$(PERLEXE) -I..\lib ..\utils\perlbug -nok -s "(UNINSTALLED)"

nokfile: utils
	$(PERLEXE) -I..\lib ..\utils\perlbug -nok -s "(UNINSTALLED)" -F perl.nok
