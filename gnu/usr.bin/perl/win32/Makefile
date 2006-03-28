#
# Makefile to build perl on Windows NT using Microsoft NMAKE.
# Supported compilers:
#	Visual C++ 2.0 through 7.0 (and possibly newer versions)
#	MS Platform SDK 64-bit compiler and tools **experimental**
#
# This is set up to build a perl.exe that runs off a shared library
# (perl58.dll).  Also makes individual DLLs for the XS extensions.
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
# Comment this out if you DON'T want your perl installation to be versioned.
# This means that the new installation will overwrite any files from the
# old installation at the same INST_TOP location.  Leaving it enabled is
# the safest route, as perl adds the extra version directory to all the
# locations it installs files to.  If you disable it, an alternative
# versioned installation can be obtained by setting INST_TOP above to a
# path that includes an arbitrary version string.
#
#INST_VER	= \5.8.8

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
# uncomment to enable multiple interpreters.  This is need for fork()
# emulation and for thread support.
#
USE_MULTI	= define

#
# Beginnings of interpreter cloning/threads; now reasonably complete.
# This should be enabled to get the fork() emulation.  This needs
# USE_MULTI as well.
#
USE_ITHREADS	= define

#
# uncomment to enable the implicit "host" layer for all system calls
# made by perl.  This needs USE_MULTI above.  This is also needed to
# get fork().
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
# WARNING! This option is deprecated and will eventually go away (enable
# USE_ITHREADS instead).
#
# uncomment to enable threads-capabilities.  This is incompatible with
# USE_ITHREADS, and is only here for people who may have come to rely
# on the experimental Thread support that was in 5.005.
#
#USE_5005THREADS	= define

#
# uncomment exactly one of the following
#
# Visual C++ 2.x
#CCTYPE		= MSVC20
# Visual C++ > 2.x and < 6.x
#CCTYPE		= MSVC
# Visual C++ 6.x (aka Visual Studio 98)
CCTYPE		= MSVC60
# Visual C++ Toolkit 2003 (free version of Visual C++ 7.x command-line tools)
#CCTYPE		= MSVC70FREE
# Visual C++ 7.x (aka Visual Studio .NET 2003) (full version)
#CCTYPE		= MSVC70

#
# uncomment next line if you want debug version of perl (big,slow)
# If not enabled, we automatically try to use maximum optimization
# with all compilers that are known to have a working optimizer.
#
#CFG		= Debug

#
# uncomment to enable use of PerlCRT.DLL when using the Visual C compiler.
# It has patches that fix known bugs in older versions of MSVCRT.DLL.
# This currently requires VC 5.0 with Service Pack 3 or later.
# Get it from CPAN at http://www.cpan.org/authors/id/D/DO/DOUGL/
# and follow the directions in the package to install.
#
# Not recommended if you have VC 6.x and you're not running Windows 9x.
#
#USE_PERLCRT	= define

#
# uncomment to enable linking with setargv.obj under the Visual C
# compiler. Setting this options enables perl to expand wildcards in
# arguments, but it may be harder to use alternate methods like
# File::DosGlob that are more powerful.  This option is supported only with
# Visual C.
#
#USE_SETARGV	= define

#
# if you want to have the crypt() builtin function implemented, leave this or
# CRYPT_LIB uncommented.  The fcrypt.c file named here contains a suitable
# version of des_fcrypt().
#
CRYPT_SRC	= fcrypt.c

#
# if you didn't set CRYPT_SRC and if you have des_fcrypt() available in a
# library, uncomment this, and make sure the library exists (see README.win32)
# Specify the full pathname of the library.
#
#CRYPT_LIB	= fcrypt.lib

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
#
# set the install locations of the compiler include/libraries
# Running VCVARS32.BAT is *required* when using Visual C.
# Some versions of Visual C don't define MSVCDIR in the environment,
# so you may have to set CCHOME explicitly (spaces in the path name should
# not be quoted)
#
#CCHOME		= f:\msvc20
CCHOME		= $(MSVCDIR)
CCINCDIR	= $(CCHOME)\include
CCLIBDIR	= $(CCHOME)\lib

#
# Additional compiler flags can be specified here.
#
BUILDOPT	= $(BUILDOPTEXTRA)

# Adding -DPERL_HASH_SEED_EXPLICIT will disable randomization of Perl's
# internal hash function unless the PERL_HASH_SEED environment variable is set.
# Alternatively, adding -DNO_HASH_SEED will completely disable the
# randomization feature. 
# The latter is required to maintain binary compatibility with Perl 5.8.0.
#
#BUILDOPT	= $(BUILDOPT) -DPERL_HASH_SEED_EXPLICIT
#BUILDOPT	= $(BUILDOPT) -DNO_HASH_SEED

#
#
# This should normally be disabled.  Adding -DPERL_POLLUTE enables support
# for old symbols by default, at the expense of extreme pollution.  You most
# probably just want to build modules that won't compile with
#         perl Makefile.PL POLLUTE=1
# instead of enabling this.  Please report such modules to the respective
# authors.
#
#BUILDOPT	= $(BUILDOPT) -DPERL_POLLUTE

#
# This should normally be disabled.  Enabling it will disable the File::Glob
# implementation of CORE::glob.
#
#BUILDOPT	= $(BUILDOPT) -DPERL_EXTERNAL_GLOB

#
# This should normally be disabled.  Enabling it causes perl to read scripts
# in text mode (which is the 5.005 behavior) and will break ByteLoader.
#
#BUILDOPT	= $(BUILDOPT) -DPERL_TEXTMODE_SCRIPTS

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

!IF "$(CRYPT_SRC)$(CRYPT_LIB)" == ""
D_CRYPT		= undef
!ELSE
D_CRYPT		= define
CRYPT_FLAG	= -DHAVE_DES_FCRYPT
!ENDIF

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

!IF "$(USE_5005THREADS)" == ""
USE_5005THREADS	= undef
!ENDIF

!IF "$(USE_5005THREADS)" == "define"
USE_ITHREADS	= undef
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

!IF "$(USE_PERLCRT)" == ""
USE_PERLCRT	= undef
!ENDIF

!IF "$(USE_IMP_SYS)$(USE_MULTI)$(USE_5005THREADS)" == "defineundefundef"
USE_MULTI	= define
!ENDIF

!IF "$(USE_ITHREADS)$(USE_MULTI)" == "defineundef"
USE_MULTI	= define
USE_5005THREADS	= undef
!ENDIF

!IF "$(USE_SITECUST)" == "define"
BUILDOPT	= $(BUILDOPT) -DUSE_SITECUSTOMIZE
!ENDIF

!IF "$(USE_MULTI)$(USE_5005THREADS)" != "undefundef"
BUILDOPT	= $(BUILDOPT) -DPERL_IMPLICIT_CONTEXT
!ENDIF

!IF "$(USE_IMP_SYS)" != "undef"
BUILDOPT	= $(BUILDOPT) -DPERL_IMPLICIT_SYS
!ENDIF

!IF "$(PROCESSOR_ARCHITECTURE)" == ""
PROCESSOR_ARCHITECTURE	= x86
!ENDIF

!IF "$(WIN64)" == ""
!IF "$(PROCESSOR_ARCHITEW6432)" != ""
PROCESSOR_ARCHITECTURE	= $(PROCESSOR_ARCHITEW6432)
WIN64			= define
!ELSE
!IF "$(PROCESSOR_ARCHITECTURE)" == "IA64"
WIN64			= define
!ELSE
WIN64			= undef
!ENDIF
!ENDIF
!ENDIF

!IF "$(USE_5005THREADS)" == "define"
ARCHNAME	= MSWin32-$(PROCESSOR_ARCHITECTURE)-thread
!ELSE
!IF "$(USE_MULTI)" == "define"
ARCHNAME	= MSWin32-$(PROCESSOR_ARCHITECTURE)-multi
!ELSE
!IF "$(USE_PERLIO)" == "define"
ARCHNAME	= MSWin32-$(PROCESSOR_ARCHITECTURE)-perlio
!ELSE
ARCHNAME	= MSWin32-$(PROCESSOR_ARCHITECTURE)
!ENDIF
!ENDIF
!ENDIF

!IF "$(USE_PERLIO)" == "define"
BUILDOPT	= $(BUILDOPT) -DUSE_PERLIO
!ENDIF

!IF "$(USE_ITHREADS)" == "define"
ARCHNAME	= $(ARCHNAME)-thread
!ENDIF

# Visual Studio 98 and .NET 2003 specific
# VC++ 6.x and 7.x can load DLL's on demand.  Makes the test suite run in
# about 10% less time.  (The free version of 7.x can't do this, however.)
!IF "$(CCTYPE)" == "MSVC60" || "$(CCTYPE)" == "MSVC70"
DELAYLOAD	= -DELAYLOAD:ws2_32.dll -DELAYLOAD:shell32.dll delayimp.lib
!ENDIF

ARCHDIR		= ..\lib\$(ARCHNAME)
COREDIR		= ..\lib\CORE
AUTODIR		= ..\lib\auto
LIBDIR		= ..\lib
EXTDIR		= ..\ext
PODDIR		= ..\pod
EXTUTILSDIR	= $(LIBDIR)\ExtUtils

#
INST_SCRIPT	= $(INST_TOP)$(INST_VER)\bin
INST_BIN	= $(INST_SCRIPT)$(INST_ARCH)
INST_LIB	= $(INST_TOP)$(INST_VER)\lib
INST_ARCHLIB	= $(INST_LIB)$(INST_ARCH)
INST_COREDIR	= $(INST_ARCHLIB)\CORE
INST_POD	= $(INST_LIB)\pod
INST_HTML	= $(INST_TOP)$(INST_VER)\html

#
# Programs to compile, build .lib files and link
#

CC		= cl
LINK32		= link
LIB32		= $(LINK32) -lib
RSC		= rc

#
# Options
#

INCLUDES	= -I$(COREDIR) -I.\include -I. -I..
#PCHFLAGS	= -Fpc:\temp\vcmoduls.pch -YX
DEFINES		= -DWIN32 -D_CONSOLE -DNO_STRICT $(CRYPT_FLAG)
LOCDEFS		= -DPERLDLL -DPERL_CORE
SUBSYS		= console
CXX_FLAG	= -TP -GX

!IF "$(USE_PERLCRT)" != "define"
LIBC	= msvcrt.lib
!ELSE
LIBC	= PerlCRT.lib
!ENDIF

!IF  "$(CFG)" == "Debug"
!  IF "$(CCTYPE)" == "MSVC20"
OPTIMIZE	= -Od -MD -Z7 -DDEBUGGING
!  ELSE
OPTIMIZE	= -Od -MD -Zi -DDEBUGGING
!  ENDIF
LINK_DBG	= -debug
!ELSE
OPTIMIZE	= -MD -Zi -DNDEBUG
# we enable debug symbols in release builds also
LINK_DBG	= -debug -opt:ref,icf
# you may want to enable this if you want COFF symbols in the executables
# in addition to the PDB symbols.  The default Dr. Watson that ships with
# Windows can use the the former but not latter.  The free WinDbg can be
# installed to get better stack traces from just the PDB symbols, so we
# avoid the bloat of COFF symbols by default.
#LINK_DBG	= $(LINK_DBG) -debugtype:both
!  IF "$(WIN64)" == "define"
# enable Whole Program Optimizations (WPO) and Link Time Code Generation (LTCG)
OPTIMIZE	= $(OPTIMIZE) -Ox -GL
LINK_DBG	= $(LINK_DBG) -ltcg
!  ELSE
# -O1 yields smaller code, which turns out to be faster than -O2 on x86
OPTIMIZE	= $(OPTIMIZE) -O1
#OPTIMIZE	= $(OPTIMIZE) -O2
!  ENDIF
!ENDIF

!IF "$(WIN64)" == "define"
DEFINES		= $(DEFINES) -DWIN64 -DCONSERVATIVE
OPTIMIZE	= $(OPTIMIZE) -Wp64 -Op
!ENDIF

# the string-pooling option -Gf is deprecated in VC++ 7.x and will be removed
# in later versions, so use read-only string-pooling (-GF) instead
!IF "$(CCTYPE)" == "MSVC70FREE" || "$(CCTYPE)" == "MSVC70"
STRPOOL		= -GF
!ELSE
STRPOOL		= -Gf
!ENDIF

!IF "$(USE_PERLCRT)" != "define"
BUILDOPT	= $(BUILDOPT) -DPERL_MSVCRT_READFIX
!ENDIF

LIBBASEFILES	= $(CRYPT_LIB) \
		oldnames.lib kernel32.lib user32.lib gdi32.lib winspool.lib \
		comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib \
		netapi32.lib uuid.lib ws2_32.lib mpr.lib winmm.lib \
		version.lib

# win64 doesn't have some libs
!IF "$(WIN64)" != "define"
LIBBASEFILES	= $(LIBBASEFILES) odbc32.lib odbccp32.lib
!ENDIF

# we add LIBC here, since we may be using PerlCRT.dll
LIBFILES	= $(LIBBASEFILES) $(LIBC)

EXTRACFLAGS	= -nologo $(STRPOOL) -W3
CFLAGS		= $(EXTRACFLAGS) $(INCLUDES) $(DEFINES) $(LOCDEFS) \
		$(PCHFLAGS) $(OPTIMIZE)
LINK_FLAGS	= -nologo -nodefaultlib $(LINK_DBG) \
		-libpath:"$(INST_COREDIR)" \
		-machine:$(PROCESSOR_ARCHITECTURE)
OBJOUT_FLAG	= -Fo
EXEOUT_FLAG	= -Fe

CFLAGS_O	= $(CFLAGS) $(BUILDOPT)

#################### do not edit below this line #######################
############# NO USER-SERVICEABLE PARTS BEYOND THIS POINT ##############

o = .obj

#
# Rules
#

.SUFFIXES : .c $(o) .dll .lib .exe .rc .res

.c$(o):
	$(CC) -c -I$(<D) $(CFLAGS_O) $(OBJOUT_FLAG)$@ $<

.y.c:
	$(NOOP)

$(o).dll:
	$(LINK32) -dll -subsystem:windows -implib:$(*B).lib -def:$(*B).def \
	    -out:$@ $(LINK_FLAGS) $(LIBFILES) $< $(LIBPERL)

.rc.res:
	$(RSC) -i.. $<

#
# various targets

# makedef.pl must be updated if this changes, and this should normally
# only change when there is an incompatible revision of the public API.
PERLIMPLIB	= ..\perl58.lib
PERLDLL		= ..\perl58.dll

MINIPERL	= ..\miniperl.exe
MINIDIR		= .\mini
PERLEXE		= ..\perl.exe
WPERLEXE	= ..\wperl.exe
GLOBEXE		= ..\perlglob.exe
CONFIGPM	= ..\lib\Config.pm ..\lib\Config_heavy.pl
MINIMOD		= ..\lib\ExtUtils\Miniperl.pm
X2P		= ..\x2p\a2p.exe

# Unicode data files generated by mktables
UNIDATAFILES	 = ..\lib\unicore\Canonical.pl ..\lib\unicore\Exact.pl \
		   ..\lib\unicore\Properties ..\lib\unicore\Decomposition.pl \
		   ..\lib\unicore\CombiningClass.pl ..\lib\unicore\Name.pl \
		   ..\lib\unicore\PVA.pl

# Directories of Unicode data files generated by mktables
UNIDATADIR1	= ..\lib\unicore\To
UNIDATADIR2	= ..\lib\unicore\lib

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
		..\utils\dprofpp	\
		..\utils\perlbug	\
		..\utils\pl2pm 		\
		..\utils\c2ph		\
		..\utils\pstruct	\
		..\utils\h2xs		\
		..\utils\perldoc	\
		..\utils\perlcc		\
		..\utils\perlivp	\
		..\utils\libnetcfg	\
		..\utils\enc2xs		\
		..\utils\piconv		\
		..\utils\cpan		\
		..\utils\xsubpp		\
		..\utils\prove		\
		..\utils\instmodsh	\
		..\pod\checkpods	\
		..\pod\pod2html		\
		..\pod\pod2latex	\
		..\pod\pod2man		\
		..\pod\pod2text		\
		..\pod\pod2usage	\
		..\pod\podchecker	\
		..\pod\podselect	\
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

!IF "$(WIN64)" == "define"
CFGSH_TMPL	= config.vc64
CFGH_TMPL	= config_H.vc64
!ELSE
CFGSH_TMPL	= config.vc
CFGH_TMPL	= config_H.vc
!ENDIF

XCOPY		= xcopy /f /r /i /d
RCOPY		= xcopy /f /r /i /e /d
NOOP		= @rem
NULL		=

DEL		= del

#
# filenames given to xsubpp must have forward slashes (since it puts
# full pathnames in #line strings)
XSUBPP		= ..\$(MINIPERL) -I..\..\lib ..\$(EXTUTILSDIR)\xsubpp \
		-C++ -prototypes

MICROCORE_SRC	=		\
		..\av.c		\
		..\deb.c	\
		..\doio.c	\
		..\doop.c	\
		..\dump.c	\
		..\globals.c	\
		..\gv.c		\
		..\hv.c		\
		..\locale.c	\
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
		..\util.c	\
		..\xsutils.c

EXTRACORE_SRC	= $(EXTRACORE_SRC) perllib.c

!IF "$(PERL_MALLOC)" == "define"
EXTRACORE_SRC	= $(EXTRACORE_SRC) ..\malloc.c
!ENDIF

EXTRACORE_SRC	= $(EXTRACORE_SRC) ..\perlio.c

WIN32_SRC	=		\
		.\win32.c	\
		.\win32sck.c	\
		.\win32thread.c

# We need this for miniperl build unless we override canned 
# config.h #define building mini\*
#!IF "$(USE_PERLIO)" == "define"
WIN32_SRC	= $(WIN32_SRC) .\win32io.c
#!ENDIF

!IF "$(CRYPT_SRC)" != ""
WIN32_SRC	= $(WIN32_SRC) .\$(CRYPT_SRC)
!ENDIF

DLL_SRC		= $(DYNALOADER).c

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
		..\iperlsys.h	\
		..\mg.h		\
		..\nostdio.h	\
		..\op.h		\
		..\opcode.h	\
		..\perl.h	\
		..\perlapi.h	\
		..\perlsdio.h	\
		..\perlsfio.h	\
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
		..\thrdvar.h	\
		.\include\dirent.h	\
		.\include\netdb.h	\
		.\include\sys\socket.h	\
		.\win32.h

CORE_H		= $(CORE_NOCFG_H) .\config.h

MICROCORE_OBJ	= $(MICROCORE_SRC:.c=.obj)
CORE_OBJ	= $(MICROCORE_OBJ) $(EXTRACORE_SRC:.c=.obj)
WIN32_OBJ	= $(WIN32_SRC:.c=.obj)
MINICORE_OBJ	= $(MICROCORE_OBJ:..\=.\mini\)	\
		  $(MINIDIR)\miniperlmain$(o)	\
		  $(MINIDIR)\perlio$(o)
MINIWIN32_OBJ	= $(WIN32_OBJ:.\=.\mini\)
MINI_OBJ	= $(MINICORE_OBJ) $(MINIWIN32_OBJ)
DLL_OBJ		= $(DLL_SRC:.c=.obj)
X2P_OBJ		= $(X2P_SRC:.c=.obj)

PERLDLL_OBJ	= $(CORE_OBJ)
PERLEXE_OBJ	= perlmain$(o)

PERLDLL_OBJ	= $(PERLDLL_OBJ) $(WIN32_OBJ) $(DLL_OBJ)
#PERLEXE_OBJ	= $(PERLEXE_OBJ) $(WIN32_OBJ) $(DLL_OBJ)

!IF "$(USE_SETARGV)" != ""
SETARGV_OBJ	= setargv$(o)
!ENDIF

# specify static extensions here
#STATIC_EXT	= Cwd Compress/Zlib
STATIC_EXT	= 

DYNALOADER		= $(EXTDIR)\DynaLoader\DynaLoader
SOCKET			= $(EXTDIR)\Socket\Socket
FCNTL			= $(EXTDIR)\Fcntl\Fcntl
OPCODE			= $(EXTDIR)\Opcode\Opcode
SDBM_FILE		= $(EXTDIR)\SDBM_File\SDBM_File
IO			= $(EXTDIR)\IO\IO
POSIX			= $(EXTDIR)\POSIX\POSIX
ATTRS			= $(EXTDIR)\attrs\attrs
THREAD			= $(EXTDIR)\Thread\Thread
B			= $(EXTDIR)\B\B
RE			= $(EXTDIR)\re\re
DUMPER			= $(EXTDIR)\Data\Dumper\Dumper
ERRNO			= $(EXTDIR)\Errno\Errno
PEEK			= $(EXTDIR)\Devel\Peek\Peek
BYTELOADER		= $(EXTDIR)\ByteLoader\ByteLoader
DPROF			= $(EXTDIR)\Devel\DProf\DProf
GLOB			= $(EXTDIR)\File\Glob\Glob
HOSTNAME		= $(EXTDIR)\Sys\Hostname\Hostname
STORABLE		= $(EXTDIR)\Storable\Storable
FILTER			= $(EXTDIR)\Filter\Util\Call\Call
ENCODE			= $(EXTDIR)\Encode\Encode
MD5			= $(EXTDIR)\Digest\MD5\MD5
PERLIOSCALAR		= $(EXTDIR)\PerlIO\scalar\scalar
MIMEBASE64		= $(EXTDIR)\MIME\Base64\Base64
TIMEHIRES		= $(EXTDIR)\Time\HiRes\HiRes
CWD			= $(EXTDIR)\Cwd\Cwd
LISTUTIL  		= $(EXTDIR)\List\Util\Util
PERLIOVIA		= $(EXTDIR)\PerlIO\via\via
XSAPITEST		= $(EXTDIR)\XS\APItest\APItest
XSTYPEMAP		= $(EXTDIR)\XS\Typemap\Typemap
UNICODENORMALIZE	= $(EXTDIR)\Unicode\Normalize\Normalize
WIN32_DIR		= ext\Win32

SOCKET_DLL		= $(AUTODIR)\Socket\Socket.dll
FCNTL_DLL		= $(AUTODIR)\Fcntl\Fcntl.dll
OPCODE_DLL		= $(AUTODIR)\Opcode\Opcode.dll
SDBM_FILE_DLL		= $(AUTODIR)\SDBM_File\SDBM_File.dll
IO_DLL			= $(AUTODIR)\IO\IO.dll
POSIX_DLL		= $(AUTODIR)\POSIX\POSIX.dll
ATTRS_DLL		= $(AUTODIR)\attrs\attrs.dll
THREAD_DLL		= $(AUTODIR)\Thread\Thread.dll
B_DLL			= $(AUTODIR)\B\B.dll
DUMPER_DLL		= $(AUTODIR)\Data\Dumper\Dumper.dll
PEEK_DLL		= $(AUTODIR)\Devel\Peek\Peek.dll
RE_DLL			= $(AUTODIR)\re\re.dll
BYTELOADER_DLL		= $(AUTODIR)\ByteLoader\ByteLoader.dll
DPROF_DLL		= $(AUTODIR)\Devel\DProf\DProf.dll
GLOB_DLL		= $(AUTODIR)\File\Glob\Glob.dll
HOSTNAME_DLL		= $(AUTODIR)\Sys\Hostname\Hostname.dll
STORABLE_DLL		= $(AUTODIR)\Storable\Storable.dll
FILTER_DLL		= $(AUTODIR)\Filter\Util\Call\Call.dll
ENCODE_DLL		= $(AUTODIR)\Encode\Encode.dll
MD5_DLL			= $(AUTODIR)\Digest\MD5\MD5.dll
PERLIOSCALAR_DLL	= $(AUTODIR)\PerlIO\scalar\scalar.dll
MIMEBASE64_DLL		= $(AUTODIR)\MIME\Base64\Base64.dll
TIMEHIRES_DLL		= $(AUTODIR)\Time\HiRes\HiRes.dll
CWD_DLL			= $(AUTODIR)\Cwd\Cwd.dll
LISTUTIL_DLL		= $(AUTODIR)\List\Util\Util.dll
PERLIOVIA_DLL		= $(AUTODIR)\PerlIO\via\via.dll
XSAPITEST_DLL		= $(AUTODIR)\XS\APItest\APItest.dll
XSTYPEMAP_DLL		= $(AUTODIR)\XS\Typemap\Typemap.dll
UNICODENORMALIZE_DLL	= $(AUTODIR)\Unicode\Normalize\Normalize.dll
WIN32_DLL		= $(AUTODIR)\Win32\Win32.dll

EXTENSION_C	= 		\
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
		$(PEEK).c	\
		$(B).c		\
		$(BYTELOADER).c	\
		$(DPROF).c	\
		$(GLOB).c	\
		$(HOSTNAME).c	\
		$(STORABLE).c	\
		$(FILTER).c	\
		$(ENCODE).c	\
		$(MD5).c	\
		$(PERLIOSCALAR).c	\
		$(MIMEBASE64).c	\
		$(TIMEHIRES).c	\
		$(CWD).c	\
		$(LISTUTIL).c	\
		$(PERLIOVIA).c	\
		$(XSAPITEST).c	\
		$(XSTYPEMAP).c	\
		$(UNICODENORMALIZE).c	\
		$(WIN32_DIR).c

EXTENSION_DLL	= 		\
		$(SOCKET_DLL)	\
		$(FCNTL_DLL)	\
		$(OPCODE_DLL)	\
		$(SDBM_FILE_DLL)\
		$(IO_DLL)	\
		$(POSIX_DLL)	\
		$(ATTRS_DLL)	\
		$(DUMPER_DLL)	\
		$(PEEK_DLL)	\
		$(B_DLL)	\
		$(RE_DLL)	\
		$(THREAD_DLL)	\
		$(BYTELOADER_DLL)	\
		$(DPROF_DLL)	\
		$(GLOB_DLL)	\
		$(HOSTNAME_DLL)	\
		$(STORABLE_DLL)	\
		$(FILTER_DLL)   \
		$(ENCODE_DLL)   \
		$(MD5_DLL)	\
		$(PERLIOSCALAR_DLL) \
		$(MIMEBASE64_DLL) \
		$(TIMEHIRES_DLL)  \
		$(CWD_DLL)	\
		$(LISTUTIL_DLL)	\
		$(PERLIOVIA_DLL)	\
		$(XSAPITEST_DLL)	\
		$(XSTYPEMAP_DLL)	\
		$(UNICODENORMALIZE_DLL)	\
		$(WIN32_DLL)

POD2HTML	= $(PODDIR)\pod2html
POD2MAN		= $(PODDIR)\pod2man
POD2LATEX	= $(PODDIR)\pod2latex
POD2TEXT	= $(PODDIR)\pod2text

CFG_VARS	=					\
		"INST_DRV=$(INST_DRV)"			\
		"INST_TOP=$(INST_TOP)"			\
		"INST_VER=$(INST_VER)"			\
		"INST_ARCH=$(INST_ARCH)"		\
		"archname=$(ARCHNAME)"			\
		"cc=$(CC)"				\
		"ld=$(LINK32)"				\
		"ccflags=$(EXTRACFLAGS) $(OPTIMIZE:"=\") $(DEFINES) $(BUILDOPT)"	\
		"cf_email=$(EMAIL)"	 		\
		"d_crypt=$(D_CRYPT)"			\
		"d_mymalloc=$(PERL_MALLOC)"		\
		"libs=$(LIBFILES)"			\
		"incpath=$(CCINCDIR:"=\")"		\
		"libperl=$(PERLIMPLIB:..\=)"		\
		"libpth=$(CCLIBDIR:"=\");$(EXTRALIBDIRS:"=\")"	\
		"libc=$(LIBC)"				\
		"make=$(MAKE_BARE)"				\
		"static_ext=$(STATIC_EXT)"		\
		"usethreads=$(USE_ITHREADS)"		\
		"use5005threads=$(USE_5005THREADS)"	\
		"useithreads=$(USE_ITHREADS)"		\
		"usemultiplicity=$(USE_MULTI)"		\
		"useperlio=$(USE_PERLIO)"		\
		"uselargefiles=$(USE_LARGE_FILES)"	\
		"usesitecustomize=$(USE_SITECUST)"	\
		"LINK_FLAGS=$(LINK_FLAGS:"=\")"		\
		"optimize=$(OPTIMIZE:"=\")"

#
# Top targets
#

all : .\config.h $(GLOBEXE) $(MINIMOD) $(CONFIGPM) $(UNIDATAFILES) $(PERLEXE) \
	$(X2P) Extensions
	@echo	Everything is up to date. '$(MAKE_BARE) test' to run test suite.

$(DYNALOADER)$(o) : $(DYNALOADER).c $(CORE_H) $(EXTDIR)\DynaLoader\dlutils.c

#------------------------------------------------------------

$(GLOBEXE) : perlglob$(o)
	$(LINK32) $(LINK_FLAGS) $(LIBFILES) -out:$@ -subsystem:$(SUBSYS) \
	    perlglob$(o) setargv$(o)

perlglob$(o)  : perlglob.c

config.w32 : $(CFGSH_TMPL)
	copy $(CFGSH_TMPL) config.w32

.\config.h : $(CFGH_TMPL)
	-del /f config.h
	copy $(CFGH_TMPL) config.h

..\config.sh : config.w32 $(MINIPERL) config_sh.PL
	$(MINIPERL) -I..\lib config_sh.PL $(CFG_VARS) config.w32 > ..\config.sh

# this target is for when changes to the main config.sh happen
# edit config.{b,v,g}c and make this target once for each supported
# compiler (e.g. `nmake CCTYPE=BORLAND regen_config_h`)
regen_config_h:
	perl config_sh.PL $(CFG_VARS) $(CFGSH_TMPL) > ..\config.sh
	cd ..
	-del /f perl.exe perl*.dll
	perl configpm
	cd win32
	-del /f $(CFGH_TMPL)
	-mkdir $(COREDIR)
	-perl config_h.PL "INST_VER=$(INST_VER)"
	rename config.h $(CFGH_TMPL)

$(CONFIGPM) : $(MINIPERL) ..\config.sh config_h.PL ..\minimod.pl
	cd ..
	miniperl configpm
	cd win32
	if exist lib\* $(RCOPY) lib\*.* ..\lib\$(NULL)
	$(XCOPY) ..\*.h $(COREDIR)\*.*
	$(XCOPY) ..\*.inc $(COREDIR)\*.*
	$(XCOPY) *.h $(COREDIR)\*.*
	$(XCOPY) ..\ext\re\re.pm $(LIBDIR)\*.*
	$(RCOPY) include $(COREDIR)\*.*
	-$(MINIPERL) -I..\lib config_h.PL "INST_VER=$(INST_VER)"
	if errorlevel 1 $(MAKE) /$(MAKEFLAGS) $(CONFIGPM)

$(MINIPERL) : $(MINIDIR) $(MINI_OBJ)
	$(LINK32) -subsystem:console -out:$@ @<<
	$(LINK_FLAGS) $(LIBFILES) $(MINI_OBJ)
<<

$(MINIDIR) :
	if not exist "$(MINIDIR)" mkdir "$(MINIDIR)"

$(MINICORE_OBJ) : $(CORE_NOCFG_H)
	$(CC) -c $(CFLAGS) -DPERL_EXTERNAL_GLOB $(OBJOUT_FLAG)$@ ..\$(*F).c

$(MINIWIN32_OBJ) : $(CORE_NOCFG_H)
	$(CC) -c $(CFLAGS) $(OBJOUT_FLAG)$@ $(*F).c

# -DPERL_IMPLICIT_SYS needs C++ for perllib.c
# This is the only file that depends on perlhost.h, vmem.h, and vdir.h
!IF "$(USE_IMP_SYS)" == "define"
perllib$(o)	: perllib.c .\perlhost.h .\vdir.h .\vmem.h
	$(CC) -c -I. $(CFLAGS_O) $(CXX_FLAG) $(OBJOUT_FLAG)$@ perllib.c
!ENDIF

# 1. we don't want to rebuild miniperl.exe when config.h changes
# 2. we don't want to rebuild miniperl.exe with non-default config.h
$(MINI_OBJ)	: $(CORE_NOCFG_H)

$(WIN32_OBJ)	: $(CORE_H)
$(CORE_OBJ)	: $(CORE_H)
$(DLL_OBJ)	: $(CORE_H)
$(X2P_OBJ)	: $(CORE_H)

perldll.def : $(MINIPERL) $(CONFIGPM) ..\global.sym ..\pp.sym ..\makedef.pl
	$(MINIPERL) -I..\lib buildext.pl --create-perllibst-h
	$(MINIPERL) -w ..\makedef.pl PLATFORM=win32 $(OPTIMIZE) $(DEFINES) $(BUILDOPT) \
	    CCTYPE=$(CCTYPE) > perldll.def

$(PERLDLL): perldll.def $(PERLDLL_OBJ) $(PERLDLL_RES) Extensions_static
	$(LINK32) -dll -def:perldll.def -base:0x28000000 -out:$@ @Extensions_static @<<
		$(LINK_FLAGS) $(DELAYLOAD) $(LIBFILES) $(PERLDLL_OBJ) $(PERLDLL_RES)
<<
	$(XCOPY) $(PERLIMPLIB) $(COREDIR)

$(PERLEXE_ICO): $(MINIPERL) makeico.pl
	$(MINIPERL) makeico.pl > $@

$(PERLEXE_RES): perlexe.rc $(PERLEXE_ICO)

$(MINIMOD) : $(MINIPERL) ..\minimod.pl
	cd ..
	miniperl minimod.pl > lib\ExtUtils\Miniperl.pm
	cd win32

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
	$(LINK32) -subsystem:console -out:$@ @<<
		$(LINK_FLAGS) $(LIBFILES) $(X2P_OBJ)
<<

perlmain.c : runperl.c
	copy runperl.c perlmain.c

perlmain$(o) : perlmain.c
	$(CC) $(CFLAGS_O) -UPERLDLL $(OBJOUT_FLAG)$@ -c perlmain.c

$(PERLEXE): $(PERLDLL) $(CONFIGPM) $(PERLEXE_OBJ) $(PERLEXE_RES)
	$(LINK32) -subsystem:console -out:$@ -stack:0x1000000 $(LINK_FLAGS) \
	    $(LIBFILES) $(PERLEXE_OBJ) $(SETARGV_OBJ) $(PERLIMPLIB) $(PERLEXE_RES)
	copy $(PERLEXE) $(WPERLEXE)
	$(MINIPERL) -I..\lib bin\exetype.pl $(WPERLEXE) WINDOWS
	copy splittree.pl ..
	$(MINIPERL) -I..\lib ..\splittree.pl "../LIB" $(AUTODIR)

$(DYNALOADER).c: $(MINIPERL) $(EXTDIR)\DynaLoader\dl_win32.xs $(CONFIGPM)
	if not exist $(AUTODIR) mkdir $(AUTODIR)
	cd $(EXTDIR)\$(*B)
	..\$(MINIPERL) -I..\..\lib $(*B)_pm.PL
	..\$(MINIPERL) -I..\..\lib XSLoader_pm.PL
	cd ..\..\win32
	$(XCOPY) $(EXTDIR)\$(*B)\$(*B).pm $(LIBDIR)\$(NULL)
	$(XCOPY) $(EXTDIR)\$(*B)\XSLoader.pm $(LIBDIR)\$(NULL)
	cd $(EXTDIR)\$(*B)
	$(XSUBPP) dl_win32.xs > $(*B).c
	cd ..\..\win32

$(EXTDIR)\DynaLoader\dl_win32.xs: dl_win32.xs
	copy dl_win32.xs $(EXTDIR)\DynaLoader\dl_win32.xs

#-------------------------------------------------------------------------------
Extensions: buildext.pl $(PERLDEP) $(CONFIGPM)
	$(MINIPERL) -I..\lib buildext.pl $(MAKE) $(PERLDEP) $(EXTDIR) --dynamic
	$(MINIPERL) -I..\lib buildext.pl $(MAKE) $(PERLDEP) ext --dynamic

Extensions_static : buildext.pl
	$(MINIPERL) -I..\lib buildext.pl $(MAKE) $(PERLDEP) $(EXTDIR) --static
	$(MINIPERL) -I..\lib buildext.pl $(MAKE) $(PERLDEP) ext --static
	$(MINIPERL) -I..\lib buildext.pl --list-static-libs > Extensions_static

Extensions_clean: 
	-if exist $(MINIPERL) $(MINIPERL) -I..\lib buildext.pl $(MAKE) $(PERLDEP) $(EXTDIR) clean
	-if exist $(MINIPERL) $(MINIPERL) -I..\lib buildext.pl $(MAKE) $(PERLDEP) ext clean

Extensions_realclean: 
	-if exist $(MINIPERL) $(MINIPERL) -I..\lib buildext.pl $(MAKE) $(PERLDEP) $(EXTDIR) realclean
	-if exist $(MINIPERL) $(MINIPERL) -I..\lib buildext.pl $(MAKE) $(PERLDEP) ext realclean

#-------------------------------------------------------------------------------

doc: $(PERLEXE)
	$(PERLEXE) -I..\lib ..\installhtml --podroot=.. --htmldir=./html \
	    --podpath=pod:lib:ext:utils --htmlroot="file://$(INST_HTML::=|)" \
	    --libpod=perlfunc:perlguts:perlvar:perlrun:perlop --recurse

# Note that this next section is parsed (and regenerated) by pod/buildtoc
# so please check that script before making structural changes here

utils: $(PERLEXE) $(X2P)
	cd ..\utils
	$(MAKE) PERL=$(MINIPERL)
	cd ..\pod
	copy ..\vms\perlvms.pod .\perlvms.pod
	copy ..\README.aix      ..\pod\perlaix.pod
	copy ..\README.amiga    ..\pod\perlamiga.pod
	copy ..\README.apollo   ..\pod\perlapollo.pod
	copy ..\README.beos     ..\pod\perlbeos.pod
	copy ..\README.bs2000   ..\pod\perlbs2000.pod
	copy ..\README.ce       ..\pod\perlce.pod
	copy ..\README.cn       ..\pod\perlcn.pod
	copy ..\README.cygwin   ..\pod\perlcygwin.pod
	copy ..\README.dgux     ..\pod\perldgux.pod
	copy ..\README.dos      ..\pod\perldos.pod
	copy ..\README.epoc     ..\pod\perlepoc.pod
	copy ..\README.freebsd  ..\pod\perlfreebsd.pod
	copy ..\README.hpux     ..\pod\perlhpux.pod
	copy ..\README.hurd     ..\pod\perlhurd.pod
	copy ..\README.irix     ..\pod\perlirix.pod
	copy ..\README.jp       ..\pod\perljp.pod
	copy ..\README.ko       ..\pod\perlko.pod
	copy ..\README.linux    ..\pod\perllinux.pod
	copy ..\README.machten  ..\pod\perlmachten.pod
	copy ..\README.macos    ..\pod\perlmacos.pod
	copy ..\README.macosx   ..\pod\perlmacosx.pod
	copy ..\README.mint     ..\pod\perlmint.pod
	copy ..\README.mpeix    ..\pod\perlmpeix.pod
	copy ..\README.netware  ..\pod\perlnetware.pod
	copy ..\README.openbsd  ..\pod\perlopenbsd.pod
	copy ..\README.os2      ..\pod\perlos2.pod
	copy ..\README.os390    ..\pod\perlos390.pod
	copy ..\README.os400    ..\pod\perlos400.pod
	copy ..\README.plan9    ..\pod\perlplan9.pod
	copy ..\README.qnx      ..\pod\perlqnx.pod
	copy ..\README.solaris  ..\pod\perlsolaris.pod
	copy ..\README.tru64    ..\pod\perltru64.pod
	copy ..\README.tw       ..\pod\perltw.pod
	copy ..\README.uts      ..\pod\perluts.pod
	copy ..\README.vmesa    ..\pod\perlvmesa.pod
	copy ..\README.vms      ..\pod\perlvms.pod
	copy ..\README.vos      ..\pod\perlvos.pod
	copy ..\README.win32    ..\pod\perlwin32.pod
	copy ..\pod\perl588delta.pod ..\pod\perldelta.pod
	$(MAKE) -f ..\win32\pod.mak converters
	cd ..\lib
	$(PERLEXE) lib_pm.PL
	cd ..\win32
	$(PERLEXE) $(PL2BAT) $(UTILS)

# Note that the pod cleanup in this next section is parsed (and regenerated
# by pod/buildtoc so please check that script before making changes here

# the doubled rmdir calls are needed because older cmd shells
# don't understand /q
distclean: realclean
	-del /f $(MINIPERL) $(PERLEXE) $(PERLDLL) $(GLOBEXE) \
		$(PERLIMPLIB) ..\miniperl.lib $(MINIMOD)
	-del /f *.def *.map
	-del /f $(EXTENSION_DLL)
	-del /f $(EXTENSION_C) $(DYNALOADER).c $(ERRNO).pm
	-del /f $(EXTDIR)\DynaLoader\dl_win32.xs
	-del /f $(EXTDIR)\DynaLoader\DynaLoader.pm
	-del /f $(EXTDIR)\DynaLoader\XSLoader.pm
	-del /f $(LIBDIR)\Encode.pm $(LIBDIR)\encoding.pm $(LIBDIR)\Errno.pm
	-del /f $(LIBDIR)\Config.pod $(LIBDIR)\POSIX.pod $(LIBDIR)\threads.pm
	-del /f $(LIBDIR)\.exists $(LIBDIR)\attrs.pm $(LIBDIR)\DynaLoader.pm
	-del /f $(LIBDIR)\XSLoader.pm $(LIBDIR)\lib.pm
	-del /f $(LIBDIR)\Fcntl.pm $(LIBDIR)\IO.pm $(LIBDIR)\Opcode.pm
	-del /f $(LIBDIR)\ops.pm $(LIBDIR)\Safe.pm
	-del /f $(LIBDIR)\SDBM_File.pm $(LIBDIR)\Socket.pm $(LIBDIR)\POSIX.pm
	-del /f $(LIBDIR)\B.pm $(LIBDIR)\O.pm $(LIBDIR)\re.pm
	-del /f $(LIBDIR)\ByteLoader.pm
	-del /f $(LIBDIR)\Devel\Peek.pm $(LIBDIR)\Devel\DProf.pm
	-del /f $(LIBDIR)\Devel\PPPort.pm
	-del /f $(LIBDIR)\File\Glob.pm
	-del /f $(LIBDIR)\Storable.pm
	-del /f $(LIBDIR)\Digest\MD5.pm
	-del /f $(LIBDIR)\PerlIO\encoding.pm
	-del /f $(LIBDIR)\PerlIO\scalar.pm
	-del /f $(LIBDIR)\PerlIO\via.pm
	-del /f $(LIBDIR)\Sys\Hostname.pm
	-del /f $(LIBDIR)\Thread\Signal.pm $(LIBDIR)\Thread\Specific.pm
	-del /f $(LIBDIR)\threads\shared.pm
	-del /f $(LIBDIR)\Time\HiRes.pm
	-del /f $(LIBDIR)\Unicode\Normalize.pm
	-del /f $(LIBDIR)\Win32.pm
	-if exist $(LIBDIR)\IO rmdir /s /q $(LIBDIR)\IO
	-if exist $(LIBDIR)\IO rmdir /s $(LIBDIR)\IO
	-if exist $(LIBDIR)\B rmdir /s /q $(LIBDIR)\B
	-if exist $(LIBDIR)\B rmdir /s $(LIBDIR)\B
	-if exist $(LIBDIR)\Data rmdir /s /q $(LIBDIR)\Data
	-if exist $(LIBDIR)\Data rmdir /s $(LIBDIR)\Data
	-if exist $(LIBDIR)\Encode rmdir /s /q $(LIBDIR)\Encode
	-if exist $(LIBDIR)\Encode rmdir /s $(LIBDIR)\Encode
	-if exist $(LIBDIR)\Filter\Util rmdir /s /q $(LIBDIR)\Filter\Util
	-if exist $(LIBDIR)\Filter\Util rmdir /s $(LIBDIR)\Filter\Util
	-if exist $(LIBDIR)\MIME rmdir /s /q $(LIBDIR)\MIME
	-if exist $(LIBDIR)\MIME rmdir /s $(LIBDIR)\MIME
	-if exist $(LIBDIR)\List rmdir /s /q $(LIBDIR)\List
	-if exist $(LIBDIR)\List rmdir /s $(LIBDIR)\List
	-if exist $(LIBDIR)\Scalar rmdir /s /q $(LIBDIR)\Scalar
	-if exist $(LIBDIR)\Scalar rmdir /s $(LIBDIR)\Scalar
	-if exist $(LIBDIR)\Sys rmdir /s /q $(LIBDIR)\Sys
	-if exist $(LIBDIR)\Sys rmdir /s $(LIBDIR)\Sys
	-if exist $(LIBDIR)\threads rmdir /s /q $(LIBDIR)\threads
	-if exist $(LIBDIR)\threads rmdir /s $(LIBDIR)\threads
	-if exist $(LIBDIR)\XS rmdir /s /q $(LIBDIR)\XS
	-if exist $(LIBDIR)\XS rmdir /s $(LIBDIR)\XS
	-cd $(PODDIR) && del /f *.html *.bat checkpods \
	    perlaix.pod perlamiga.pod perlapollo.pod perlbeos.pod \
	    perlbs2000.pod perlce.pod perlcn.pod perlcygwin.pod \
	    perldelta.pod perldgux.pod perldos.pod perlepoc.pod \
	    perlfreebsd.pod perlhpux.pod perlhurd.pod perlirix.pod \
	    perljp.pod perlko.pod perllinux.pod perlmachten.pod \
	    perlmacos.pod perlmacosx.pod perlmint.pod perlmpeix.pod \
	    perlnetware.pod perlopenbsd.pod perlos2.pod perlos390.pod \
	    perlos400.pod perlplan9.pod perlqnx.pod perlsolaris.pod \
	    perltru64.pod perltw.pod perluts.pod perlvmesa.pod perlvms.pod \
	    perlvms.pod perlvos.pod perlwin32.pod \
	    pod2html pod2latex pod2man pod2text pod2usage \
	    podchecker podselect
	-cd ..\utils && del /f h2ph splain perlbug pl2pm c2ph pstruct h2xs \
	    perldoc perlivp dprofpp perlcc libnetcfg enc2xs piconv cpan *.bat \
	    xsubpp instmodsh prove
	-cd ..\x2p && del /f find2perl s2p psed *.bat
	-del /f ..\config.sh ..\splittree.pl perlmain.c dlutils.c config.h.new
	-del /f $(CONFIGPM)
	-del /f bin\*.bat
	-del /f perllibst.h
	-del /f $(PERLEXE_ICO) perl.base
	-cd .. && del /s *.lib *.map *.pdb *.ilk *.bs *$(o) .exists pm_to_blib
	-cd $(EXTDIR) && del /s *.def Makefile Makefile.old
	-if exist $(AUTODIR) rmdir /s /q $(AUTODIR)
	-if exist $(AUTODIR) rmdir /s $(AUTODIR)
	-if exist $(COREDIR) rmdir /s /q $(COREDIR)
	-if exist $(COREDIR) rmdir /s $(COREDIR)

install : all installbare installhtml

installbare : utils
	$(PERLEXE) ..\installperl
	if exist $(WPERLEXE) $(XCOPY) $(WPERLEXE) $(INST_BIN)\*.*
	$(XCOPY) $(GLOBEXE) $(INST_BIN)\*.*
	if exist ..\perl*.pdb $(XCOPY) ..\perl*.pdb $(INST_BIN)\*.*
	if exist ..\x2p\a2p.pdb $(XCOPY) ..\x2p\a2p.pdb $(INST_BIN)\*.*
	$(XCOPY) bin\*.bat $(INST_SCRIPT)\*.*

installhtml : doc
	$(RCOPY) html\*.* $(INST_HTML)\*.*

inst_lib : $(CONFIGPM)
	copy splittree.pl ..
	$(MINIPERL) -I..\lib ..\splittree.pl "../LIB" $(AUTODIR)
	$(RCOPY) ..\lib $(INST_LIB)\*.*

$(UNIDATAFILES) : $(MINIPERL) $(CONFIGPM) ..\lib\unicore\mktables
	cd ..\lib\unicore && \
	..\$(MINIPERL) -I.. mktables

minitest : $(MINIPERL) $(GLOBEXE) $(CONFIGPM) utils $(UNIDATAFILES)
	$(XCOPY) $(MINIPERL) ..\t\$(NULL)
	if exist ..\t\perl.exe del /f ..\t\perl.exe
	rename ..\t\miniperl.exe perl.exe
	$(XCOPY) $(GLOBEXE) ..\t\$(NULL)
	attrib -r ..\t\*.*
	cd ..\t && \
	$(MINIPERL) -I..\lib harness base/*.t comp/*.t cmd/*.t io/*.t op/*.t pragma/*.t

test-prep : all utils
	$(XCOPY) $(PERLEXE) ..\t\$(NULL)
	$(XCOPY) $(PERLDLL) ..\t\$(NULL)
	$(XCOPY) $(GLOBEXE) ..\t\$(NULL)

test : test-prep
	cd ..\t
	$(PERLEXE) -I..\lib harness $(TEST_SWITCHES) $(TEST_FILES)
	cd ..\win32

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

# the doubled rmdir calls are needed because older cmd shells
# don't understand /q
_clean :
	-@$(DEL) miniperlmain$(o)
	-@$(DEL) $(MINIPERL)
	-@$(DEL) perlglob$(o)
	-@$(DEL) perlmain$(o)
	-@$(DEL) config.w32
	-@$(DEL) config.h
	-@$(DEL) $(GLOBEXE)
	-@$(DEL) $(PERLEXE)
	-@$(DEL) $(WPERLEXE)
	-@$(DEL) $(PERLDLL)
	-@$(DEL) $(CORE_OBJ)
	-if exist $(MINIDIR) rmdir /s /q $(MINIDIR)
	-if exist $(MINIDIR) rmdir /s $(MINIDIR)
	-if exist $(UNIDATADIR1) rmdir /s /q $(UNIDATADIR1)
	-if exist $(UNIDATADIR1) rmdir /s $(UNIDATADIR1)
	-if exist $(UNIDATADIR2) rmdir /s /q $(UNIDATADIR2)
	-if exist $(UNIDATADIR2) rmdir /s $(UNIDATADIR2)
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
