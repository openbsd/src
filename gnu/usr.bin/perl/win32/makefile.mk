#
# Makefile to build perl on Windows NT using DMAKE.
# Supported compilers:
#	Visual C++ 2.0 or later
#	Borland C++ 5.02 or later
#	MinGW with gcc-2.95.2 or later
#	MS Platform SDK 64-bit compiler and tools **experimental**
#
# This is set up to build a perl.exe that runs off a shared library
# (perl512.dll).  Also makes individual DLLs for the XS extensions.
#

##
## Make sure you read README.win32 *before* you mess with anything here!
##

##
## Build configuration.  Edit the values below to suit your needs.
##

#
# Set these to wherever you want "dmake install" to put your
# newly built perl.
#
INST_DRV	*= c:
INST_TOP	*= $(INST_DRV)\perl

#
# Uncomment if you want to build a 32-bit Perl using a 32-bit compiler
# on a 64-bit version of Windows.
#WIN64		*= undef

#
# Comment this out if you DON'T want your perl installation to be versioned.
# This means that the new installation will overwrite any files from the
# old installation at the same INST_TOP location.  Leaving it enabled is
# the safest route, as perl adds the extra version directory to all the
# locations it installs files to.  If you disable it, an alternative
# versioned installation can be obtained by setting INST_TOP above to a
# path that includes an arbitrary version string.
#
#INST_VER	*= \5.12.2

#
# Comment this out if you DON'T want your perl installation to have
# architecture specific components.  This means that architecture-
# specific files will be installed along with the architecture-neutral
# files.  Leaving it enabled is safer and more flexible, in case you
# want to build multiple flavors of perl and install them together in
# the same location.  Commenting it out gives you a simpler
# installation that is easier to understand for beginners.
#
#INST_ARCH	*= \$(ARCHNAME)

#
# Uncomment this if you want perl to run
# 	$Config{sitelibexp}\sitecustomize.pl
# before anything else.  This script can then be set up, for example,
# to add additional entries to @INC.
#
#USE_SITECUST	*= define

#
# uncomment to enable multiple interpreters.  This is need for fork()
# emulation and for thread support.
#
USE_MULTI	*= define

#
# Interpreter cloning/threads; now reasonably complete.
# This should be enabled to get the fork() emulation.  
# This needs USE_MULTI above.
#
USE_ITHREADS	*= define

#
# uncomment to enable the implicit "host" layer for all system calls
# made by perl.  This needs USE_MULTI above.  
# This is also needed to get fork().
#
USE_IMP_SYS	*= define

#
# Comment out next assign to disable perl's I/O subsystem and use compiler's 
# stdio for IO - depending on your compiler vendor and run time library you may 
# then get a number of fails from make test i.e. bugs - complain to them not us ;-). 
# You will also be unable to take full advantage of perl5.8's support for multiple 
# encodings and may see lower IO performance. You have been warned.
USE_PERLIO	*= define

#
# Comment this out if you don't want to enable large file support for
# some reason.  Should normally only be changed to maintain compatibility
# with an older release of perl.
USE_LARGE_FILES	*= define

#
# uncomment exactly one of the following
#
# Visual C++ 2.x
#CCTYPE		*= MSVC20
# Visual C++ > 2.x and < 6.x
#CCTYPE		*= MSVC
# Visual C++ 6.x (aka Visual C++ 98)
#CCTYPE		*= MSVC60
# Visual C++ Toolkit 2003 (aka Visual C++ 7.x) (free command-line tools)
#CCTYPE		*= MSVC70FREE
# Visual C++ .NET 2003 (aka Visual C++ 7.x) (full version)
#CCTYPE		*= MSVC70
# Visual C++ 2005 Express Edition (aka Visual C++ 8.x) (free version)
#CCTYPE		*= MSVC80FREE
# Visual C++ 2005 (aka Visual C++ 8.x) (full version)
#CCTYPE		*= MSVC80
# Visual C++ 2008 Express Edition (aka Visual C++ 9.x) (free version)
#CCTYPE		*= MSVC90FREE
# Visual C++ 2008 (aka Visual C++ 9.x) (full version)
#CCTYPE		*= MSVC90
# Borland 5.02 or later
#CCTYPE		*= BORLAND
# MinGW or mingw-w64 with gcc-2.95.2 or later
CCTYPE		*= GCC

#
# uncomment this if your Borland compiler is older than v5.4.
#BCCOLD		*= define
#
# uncomment this if you want to use Borland's VCL as your CRT
#BCCVCL		*= define

#
# uncomment this if you are compiling under Windows 95/98 and command.com
# (not needed if you're running under 4DOS/NT 6.01 or later)
#IS_WIN95	*= define

#
# uncomment next line if you want debug version of perl (big,slow)
# If not enabled, we automatically try to use maximum optimization
# with all compilers that are known to have a working optimizer.
#
#CFG		*= Debug

#
# uncomment to enable use of PerlCRT.DLL when using the Visual C compiler.
# It has patches that fix known bugs in older versions of MSVCRT.DLL.
# This currently requires VC 5.0 with Service Pack 3 or later.
# Get it from CPAN at http://www.cpan.org/authors/id/D/DO/DOUGL/
# and follow the directions in the package to install.
#
# Not recommended if you have VC 6.x and you're not running Windows 9x.
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
# if you want to have the crypt() builtin function implemented, leave this or
# CRYPT_LIB uncommented.  The fcrypt.c file named here contains a suitable
# version of des_fcrypt().
#
CRYPT_SRC	*= fcrypt.c

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
# if you ask for USE_IMP_SYS above.
#
#PERL_MALLOC	*= define

#
# set this to enable debugging mstats
# This must be enabled to use the Devel::Peek::mstat() function.  This cannot
# be enabled without PERL_MALLOC as well.
#
#DEBUG_MSTATS	*= define

#
# set this to additionally provide a statically linked perl-static.exe.
# Note that dynamic loading will not work with this perl, so you must
# include required modules statically using the STATIC_EXT or ALL_STATIC
# variables below. A static library perl512s.lib will also be created.
# Ordinary perl.exe is not affected by this option.
#
#BUILD_STATIC	*= define

#
# in addition to BUILD_STATIC the option ALL_STATIC makes *every*
# extension get statically built
# This will result in a very large perl executable, but the main purpose
# is to have proper linking set so as to be able to create miscellaneous
# executables with different built-in extensions
#
#ALL_STATIC	*= define

#
# set the install locations of the compiler include/libraries
# Running VCVARS32.BAT is *required* when using Visual C.
# Some versions of Visual C don't define MSVCDIR in the environment,
# so you may have to set CCHOME explicitly (spaces in the path name should
# not be quoted)
#
.IF "$(CCTYPE)" == "BORLAND"
CCHOME		*= C:\Borland\BCC55
.ELIF "$(CCTYPE)" == "GCC"
CCHOME		*= C:\MinGW
.ELSE
CCHOME		*= $(MSVCDIR)
.ENDIF

#
# If building with gcc-4.x.x (or x86_64-w64-mingw32-gcc-4.x.x), then
# uncomment  the following assignment to GCC_4XX, make sure that CCHOME
# has been set correctly above, and uncomment the appropriate
# GCCHELPERDLL line.
# The name of the dll can change, depending upon which vendor has supplied
# your 4.x.x compiler, and upon the values of "x".
# (The dll will be in your mingw/bin folder, so check there if you're
# unsure about the correct name.)
# Without these corrections, the op/taint.t test script will fail.
#
#GCC_4XX		*= define
#GCCHELPERDLL	*= $(CCHOME)\bin\libgcc_s_sjlj-1.dll
#GCCHELPERDLL	*= $(CCHOME)\bin\libgcc_s_dw2-1.dll
#GCCHELPERDLL	*= $(CCHOME)\bin\libgcc_s_1.dll

#
# uncomment this if you are using x86_64-w64-mingw32 cross-compiler
# ie if your gcc executable is called 'x86_64-w64-mingw32-gcc'
# instead of the usual 'gcc'.
#
#GCCCROSS	*= define

#
# Following sets $Config{incpath} and $Config{libpth}
#

.IF "$(GCCCROSS)" == "define"
CCINCDIR *= $(CCHOME)\mingw\include
CCLIBDIR *= $(CCHOME)\mingw\lib
.ELSE
CCINCDIR *= $(CCHOME)\include
CCLIBDIR *= $(CCHOME)\lib
.ENDIF

#
# Additional compiler flags can be specified here.
#
BUILDOPT	*= $(BUILDOPTEXTRA)

#
# Adding -DPERL_HASH_SEED_EXPLICIT will disable randomization of Perl's
# internal hash function unless the PERL_HASH_SEED environment variable is set.
# Alternatively, adding -DNO_HASH_SEED will completely disable the
# randomization feature. 
# The latter is required to maintain binary compatibility with Perl 5.8.0.
#
#BUILDOPT	+= -DPERL_HASH_SEED_EXPLICIT
#BUILDOPT	+= -DNO_HASH_SEED

#
# This should normally be disabled.  Adding -DPERL_POLLUTE enables support
# for old symbols by default, at the expense of extreme pollution.  You most
# probably just want to build modules that won't compile with
#         perl Makefile.PL POLLUTE=1
# instead of enabling this.  Please report such modules to the respective
# authors.
#
#BUILDOPT	+= -DPERL_POLLUTE

#
# This should normally be disabled.  Enabling it will disable the File::Glob
# implementation of CORE::glob.
#
#BUILDOPT	+= -DPERL_EXTERNAL_GLOB

#
# This should normally be disabled.  Enabling it causes perl to read scripts
# in text mode (which is the 5.005 behavior) and will break ByteLoader.
#
#BUILDOPT	+= -DPERL_TEXTMODE_SCRIPTS

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

PERL_MALLOC	*= undef
DEBUG_MSTATS	*= undef

USE_SITECUST	*= undef
USE_MULTI	*= undef
USE_ITHREADS	*= undef
USE_IMP_SYS	*= undef
USE_PERLIO	*= undef
USE_LARGE_FILES	*= undef
USE_PERLCRT	*= undef

.IF "$(USE_IMP_SYS)" == "define"
PERL_MALLOC	= undef
.ENDIF

.IF "$(PERL_MALLOC)" == "undef"
DEBUG_MSTATS	= undef
.ENDIF

.IF "$(DEBUG_MSTATS)" == "define"
BUILDOPT	+= -DPERL_DEBUGGING_MSTATS
.ENDIF

.IF "$(USE_IMP_SYS) $(USE_MULTI)" == "define undef"
USE_MULTI	!= define
.ENDIF

.IF "$(USE_ITHREADS) $(USE_MULTI)" == "define undef"
USE_MULTI	!= define
.ENDIF

.IF "$(USE_SITECUST)" == "define"
BUILDOPT	+= -DUSE_SITECUSTOMIZE
.ENDIF

.IF "$(USE_MULTI)" != "undef"
BUILDOPT	+= -DPERL_IMPLICIT_CONTEXT
.ENDIF

.IF "$(USE_IMP_SYS)" != "undef"
BUILDOPT	+= -DPERL_IMPLICIT_SYS
.ENDIF

.IMPORT .IGNORE : PROCESSOR_ARCHITECTURE PROCESSOR_ARCHITEW6432 WIN64

PROCESSOR_ARCHITECTURE *= x86

.IF "$(WIN64)" == ""
# When we are running from a 32bit cmd.exe on AMD64 then
# PROCESSOR_ARCHITECTURE is set to x86 and PROCESSOR_ARCHITEW6432
# is set to AMD64
.IF "$(PROCESSOR_ARCHITEW6432)" != ""
PROCESSOR_ARCHITECTURE	!= $(PROCESSOR_ARCHITEW6432)
WIN64			= define
.ELIF "$(PROCESSOR_ARCHITECTURE)" == "AMD64" || "$(PROCESSOR_ARCHITECTURE)" == "IA64"
WIN64			= define
.ELSE
WIN64			= undef
.ENDIF
.ENDIF

ARCHITECTURE = $(PROCESSOR_ARCHITECTURE)
.IF "$(ARCHITECTURE)" == "AMD64"
ARCHITECTURE	= x64
.ENDIF
.IF "$(ARCHITECTURE)" == "IA64"
ARCHITECTURE	= ia64
.ENDIF

.IF "$(USE_MULTI)" == "define"
ARCHNAME	= MSWin32-$(ARCHITECTURE)-multi
.ELSE
.IF "$(USE_PERLIO)" == "define"
ARCHNAME	= MSWin32-$(ARCHITECTURE)-perlio
.ELSE
ARCHNAME	= MSWin32-$(ARCHITECTURE)
.ENDIF
.ENDIF

.IF "$(USE_ITHREADS)" == "define"
ARCHNAME	!:= $(ARCHNAME)-thread
.ENDIF

# Visual C++ 98, .NET 2003, 2005 and 2008 specific.
# VC++ 6.x, 7.x, 8.x and 9.x can load DLL's on demand.  Makes the test suite run
# in about 10% less time.  (The free version of 7.x can't do this, but the free
# versions of 8.x and 9.x can.)
.IF "$(CCTYPE)" == "MSVC60" || "$(CCTYPE)" == "MSVC70"     || \
    "$(CCTYPE)" == "MSVC80" || "$(CCTYPE)" == "MSVC80FREE" ||
    "$(CCTYPE)" == "MSVC90" || "$(CCTYPE)" == "MSVC90FREE"
DELAYLOAD	*= -DELAYLOAD:ws2_32.dll delayimp.lib
.ENDIF

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
EXTUTILSDIR	= $(LIBDIR)\ExtUtils
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

.USESHELL :

.IF "$(CCTYPE)" == "BORLAND"

CC		= bcc32
.IF "$(BCCOLD)" != "define"
LINK32		= ilink32
.ELSE
LINK32		= tlink32
.ENDIF
LIB32		= tlib /a /P128
IMPLIB		= implib -c
RSC		= brcc32

#
# Options
#
INCLUDES	= -I$(COREDIR) -I.\include -I. -I.. -I"$(CCINCDIR)"
#PCHFLAGS	= -H -Hc -H=c:\temp\bcmoduls.pch
DEFINES		= -DWIN32 $(CRYPT_FLAG)
LOCDEFS		= -DPERLDLL -DPERL_CORE
SUBSYS		= console
CXX_FLAG	= -P

LIBC		= cw32mti.lib

# same libs as MSVC, except Borland doesn't have oldnames.lib
LIBFILES	= $(CRYPT_LIB) \
		kernel32.lib user32.lib gdi32.lib winspool.lib \
		comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib \
		netapi32.lib uuid.lib ws2_32.lib mpr.lib winmm.lib \
		version.lib odbc32.lib odbccp32.lib comctl32.lib \
		import32.lib $(LIBC)

.IF  "$(CFG)" == "Debug"
OPTIMIZE	= -v -D_RTLDLL -DDEBUGGING
LINK_DBG	= -v
.ELSE
OPTIMIZE	= -O2 -D_RTLDLL
LINK_DBG	=
.ENDIF

EXTRACFLAGS	=
CFLAGS		= -w -g0 -tWM -tWD $(INCLUDES) $(DEFINES) $(LOCDEFS) \
		$(PCHFLAGS) $(OPTIMIZE)
LINK_FLAGS	= $(LINK_DBG) -x -L"$(INST_COREDIR)" -L"$(CCLIBDIR)" \
		-L"$(CCLIBDIR)\PSDK"
OBJOUT_FLAG	= -o
EXEOUT_FLAG	= -e
LIBOUT_FLAG	=
.IF "$(BCCOLD)" != "define"
LINK_FLAGS	+= -Gn
DEFINES  += -D_MT -D__USELOCALES__ -D_WIN32_WINNT=0x0410
.END
.IF "$(BCCVCL)" == "define"
LIBC		= cp32mti.lib vcl.lib vcl50.lib vclx50.lib vcle50.lib
LINK_FLAGS	+= -L"$(CCLIBDIR)\Release"
.END


.ELIF "$(CCTYPE)" == "GCC"

.IF "$(GCCCROSS)" == "define"
ARCHPREFIX      = x86_64-w64-mingw32-
.ENDIF

CC		= $(ARCHPREFIX)gcc
LINK32		= $(ARCHPREFIX)g++
LIB32		= $(ARCHPREFIX)ar rc
IMPLIB		= $(ARCHPREFIX)dlltool
RSC		= $(ARCHPREFIX)windres

i = .i
o = .o
a = .a

#
# Options
#

INCLUDES	= -I.\include -I. -I.. -I$(COREDIR)
DEFINES		= -DWIN32 $(CRYPT_FLAG)
.IF "$(WIN64)" == "define"
DEFINES		+= -DWIN64 -DCONSERVATIVE
.ENDIF
LOCDEFS		= -DPERLDLL -DPERL_CORE
SUBSYS		= console
CXX_FLAG	= -xc++

# Current releases of MinGW 5.1.4 (as of 11-Aug-2009) will fail to link
# correctly if -lmsvcrt is specified explicitly.
LIBC		=
#LIBC		= -lmsvcrt

# same libs as MSVC
LIBFILES	= $(CRYPT_LIB) $(LIBC) \
		  -lmoldname -lkernel32 -luser32 -lgdi32 \
		  -lwinspool -lcomdlg32 -ladvapi32 -lshell32 -lole32 \
		  -loleaut32 -lnetapi32 -luuid -lws2_32 -lmpr \
		  -lwinmm -lversion -lodbc32 -lodbccp32 -lcomctl32

.IF  "$(CFG)" == "Debug"
OPTIMIZE	= -g -O2 -DDEBUGGING
LINK_DBG	= -g
.ELSE
OPTIMIZE	= -s -O2
LINK_DBG	= -s
.ENDIF

EXTRACFLAGS	=
CFLAGS		= $(INCLUDES) $(DEFINES) $(LOCDEFS) $(OPTIMIZE)
LINK_FLAGS	= $(LINK_DBG) -L"$(INST_COREDIR)" -L"$(CCLIBDIR)"
OBJOUT_FLAG	= -o
EXEOUT_FLAG	= -o
LIBOUT_FLAG	=

# NOTE: we assume that GCC uses MSVCRT.DLL
# See comments about PERL_MSVCRT_READFIX in the "cl" compiler section below.
BUILDOPT	+= -fno-strict-aliasing -mms-bitfields -DPERL_MSVCRT_READFIX

.ELSE

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
CXX_FLAG	= -TP -EHsc

.IF "$(USE_PERLCRT)" != "define"
LIBC	= msvcrt.lib
.ELSE
LIBC	= PerlCRT.lib
.ENDIF

.IF  "$(CFG)" == "Debug"
.IF "$(CCTYPE)" == "MSVC20"
OPTIMIZE	= -Od -MD -Z7 -DDEBUGGING
.ELSE
OPTIMIZE	= -O1 -MD -Zi -DDEBUGGING
.ENDIF
LINK_DBG	= -debug
.ELSE
OPTIMIZE	= -MD -Zi -DNDEBUG
# we enable debug symbols in release builds also
LINK_DBG	= -debug -opt:ref,icf
# you may want to enable this if you want COFF symbols in the executables
# in addition to the PDB symbols.  The default Dr. Watson that ships with
# Windows can use the the former but not latter.  The free WinDbg can be
# installed to get better stack traces from just the PDB symbols, so we
# avoid the bloat of COFF symbols by default.
#LINK_DBG	= $(LINK_DBG) -debugtype:both
.IF "$(WIN64)" == "define"
# enable Whole Program Optimizations (WPO) and Link Time Code Generation (LTCG)
OPTIMIZE	+= -Ox -GL
LINK_DBG	+= -ltcg
.ELSE
# -O1 yields smaller code, which turns out to be faster than -O2 on x86
OPTIMIZE	+= -O1
#OPTIMIZE	+= -O2
.ENDIF
.ENDIF

.IF "$(WIN64)" == "define"
DEFINES		+= -DWIN64 -DCONSERVATIVE
OPTIMIZE	+= -Wp64 -fp:precise
.ENDIF

# For now, silence VC++ 8.x's and 9.x's warnings about "unsafe" CRT functions
# and POSIX CRT function names being deprecated.
.IF "$(CCTYPE)" == "MSVC80" || "$(CCTYPE)" == "MSVC80FREE" || \
    "$(CCTYPE)" == "MSVC90" || "$(CCTYPE)" == "MSVC90FREE"
DEFINES		+= -D_CRT_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_DEPRECATE
.ENDIF

# Use the MSVCRT read() fix if the PerlCRT was not chosen, but only when using
# VC++ 6.x or earlier. Later versions use MSVCR70.dll, MSVCR71.dll, etc, which
# do not require the fix.
.IF "$(CCTYPE)" == "MSVC20" || "$(CCTYPE)" == "MSVC" || "$(CCTYPE)" == "MSVC60" 
.IF "$(USE_PERLCRT)" != "define"
BUILDOPT	+= -DPERL_MSVCRT_READFIX
.ENDIF
.ENDIF

LIBBASEFILES	= $(CRYPT_LIB) \
		oldnames.lib kernel32.lib user32.lib gdi32.lib winspool.lib \
		comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib \
		netapi32.lib uuid.lib ws2_32.lib mpr.lib winmm.lib \
		version.lib odbc32.lib odbccp32.lib comctl32.lib

# The 64 bit Platform SDK compilers contain a runtime library that doesn't
# include the buffer overrun verification code used by the /GS switch.
# Since the code links against libraries that are compiled with /GS, this
# "security cookie verification" must be included via bufferoverlow.lib.
.IF "$(WIN64)" == "define"
LIBBASEFILES    += bufferoverflowU.lib
.ENDIF

# we add LIBC here, since we may be using PerlCRT.dll
LIBFILES	= $(LIBBASEFILES) $(LIBC)

EXTRACFLAGS	= -nologo -GF -W3
CFLAGS		= $(EXTRACFLAGS) $(INCLUDES) $(DEFINES) $(LOCDEFS) \
		$(PCHFLAGS) $(OPTIMIZE)
LINK_FLAGS	= -nologo -nodefaultlib $(LINK_DBG) \
		-libpath:"$(INST_COREDIR)" \
		-machine:$(PROCESSOR_ARCHITECTURE)
LIB_FLAGS	= -nologo
OBJOUT_FLAG	= -Fo
EXEOUT_FLAG	= -Fe
LIBOUT_FLAG	= /out:

.ENDIF

CFLAGS_O	= $(CFLAGS) $(BUILDOPT)

.IF "$(CCTYPE)" == "MSVC80" || "$(CCTYPE)" == "MSVC80FREE" || \
    "$(CCTYPE)" == "MSVC90" || "$(CCTYPE)" == "MSVC90FREE"
LINK_FLAGS	+= "/manifestdependency:type='Win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'"
.ELSE
RSC_FLAGS	= -DINCLUDE_MANIFEST
.ENDIF


# used to allow local linking flags that are not propogated into Config.pm,
# currently unused
#   -- BKS, 12-12-1999
PRIV_LINK_FLAGS	*=
BLINK_FLAGS	= $(PRIV_LINK_FLAGS) $(LINK_FLAGS)

#################### do not edit below this line #######################
############# NO USER-SERVICEABLE PARTS BEYOND THIS POINT ##############

# Some old dmakes (including Sarathy's one at
# http://search.cpan.org/CPAN/authors/id/G/GS/GSAR/dmake-4.1pl1-win32.zip)
# don't support logical OR (||) or logical AND (&&) in conditional
# expressions and hence don't process this makefile correctly. Determine
# whether this is the case so that we can give the user an error message.
.IF 1 == 1 || 1 == 1
NEWDMAKE = define
.ELSE
NEWDMAKE = undef
.ENDIF

o *= .obj
a *= .lib

LKPRE		= INPUT (
LKPOST		= )

#
# Rules
#

.SUFFIXES : .c .i $(o) .dll $(a) .exe .rc .res

.c$(o):
	$(CC) -c $(null,$(<:d) $(NULL) -I$(<:d)) $(CFLAGS_O) $(OBJOUT_FLAG)$@ $<

.c.i:
	$(CC) -c $(null,$(<:d) $(NULL) -I$(<:d)) $(CFLAGS_O) -E $< >$@

.y.c:
	$(NOOP)

$(o).dll:
.IF "$(CCTYPE)" == "BORLAND"
	$(LINK32) -Tpd -ap $(BLINK_FLAGS) c0d32$(o) $<,$@,,$(LIBFILES),$(*B).def
	$(IMPLIB) $(*B).lib $@
.ELIF "$(CCTYPE)" == "GCC"
	$(LINK32) -o $@ $(BLINK_FLAGS) $< $(LIBFILES)
	$(IMPLIB) --input-def $(*B).def --output-lib $(*B).a $@
.ELSE
	$(LINK32) -dll -subsystem:windows -implib:$(*B).lib -def:$(*B).def \
	    -out:$@ $(BLINK_FLAGS) $(LIBFILES) $< $(LIBPERL)
	$(EMBED_DLL_MANI)
.ENDIF

.rc.res:
.IF "$(CCTYPE)" == "GCC"
	$(RSC) --use-temp-file --include-dir=. --include-dir=.. -O COFF -D INCLUDE_MANIFEST -i $< -o $@
.ELSE
	$(RSC) -i.. -DINCLUDE_MANIFEST $<
.ENDIF

#
# various targets
MINIPERL	= ..\miniperl.exe
MINIDIR		= .\mini
PERLEXE		= ..\perl.exe
WPERLEXE	= ..\wperl.exe
PERLEXESTATIC	= ..\perl-static.exe
GLOBEXE		= ..\perlglob.exe
CONFIGPM	= ..\lib\Config.pm ..\lib\Config_heavy.pl
MINIMOD		= ..\lib\ExtUtils\Miniperl.pm
X2P		= ..\x2p\a2p.exe
GENUUDMAP	= ..\generate_uudmap.exe
.IF "$(BUILD_STATIC)" == "define"
PERLSTATIC	= static
.ELSE
PERLSTATIC	= 
.ENDIF

# Unicode data files generated by mktables
UNIDATAFILES	 = ..\lib\unicore\Decomposition.pl ..\lib\unicore\TestProp.pl \
		   ..\lib\unicore\CombiningClass.pl ..\lib\unicore\Name.pl \
		   ..\lib\unicore\Heavy.pl ..\lib\unicore\mktables.lst

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
PERLDEP = perldll.def


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
		..\utils\cpanp-run-perl	\
		..\utils\cpanp	\
		..\utils\cpan2dist	\
		..\utils\shasum		\
		..\utils\instmodsh	\
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

.IF "$(CCTYPE)" == "BORLAND"

CFGSH_TMPL	= config.bc
CFGH_TMPL	= config_H.bc

.ELIF "$(CCTYPE)" == "GCC"

.IF "$(WIN64)" == "define"
.IF "$(GCCCROSS)" == "define"
CFGSH_TMPL	= config.gc64
CFGH_TMPL	= config_H.gc64
.ELSE
CFGSH_TMPL	= config.gc64nox
CFGH_TMPL	= config_H.gc64nox
.ENDIF
.ELSE
CFGSH_TMPL	= config.gc
CFGH_TMPL	= config_H.gc
.ENDIF
PERLIMPLIB	= ..\libperl512$(a)
PERLSTATICLIB	= ..\libperl512s$(a)

.ELSE

.IF "$(WIN64)" == "define"
CFGSH_TMPL	= config.vc64
CFGH_TMPL	= config_H.vc64
.ELSE
CFGSH_TMPL	= config.vc
CFGH_TMPL	= config_H.vc
.ENDIF

.ENDIF

# makedef.pl must be updated if this changes, and this should normally
# only change when there is an incompatible revision of the public API.
PERLIMPLIB	*= ..\perl512$(a)
PERLSTATICLIB	*= ..\perl512s$(a)
PERLDLL		= ..\perl512.dll

XCOPY		= xcopy /f /r /i /d /y
RCOPY		= xcopy /f /r /i /e /d /y
NOOP		= @rem

MICROCORE_SRC	=		\
		..\av.c		\
		..\deb.c	\
		..\doio.c	\
		..\doop.c	\
		..\dump.c	\
		..\globals.c	\
		..\gv.c		\
		..\mro.c	\
		..\hv.c		\
		..\locale.c	\
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

EXTRACORE_SRC	+= perllib.c

.IF "$(PERL_MALLOC)" == "define"
EXTRACORE_SRC	+= ..\malloc.c
.ENDIF

EXTRACORE_SRC	+= ..\perlio.c

WIN32_SRC	=		\
		.\win32.c	\
		.\win32sck.c	\
		.\win32thread.c

# We need this for miniperl build unless we override canned 
# config.h #define building mini\*
#.IF "$(USE_PERLIO)" == "define"
WIN32_SRC	+= .\win32io.c
#.ENDIF

.IF "$(CRYPT_SRC)" != ""
WIN32_SRC	+= .\$(CRYPT_SRC)
.ENDIF

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
		.\include\dirent.h	\
		.\include\netdb.h	\
		.\include\sys\socket.h	\
		.\win32.h

CORE_H		= $(CORE_NOCFG_H) .\config.h ..\git_version.h

UUDMAP_H	= ..\uudmap.h
BITCOUNT_H	= ..\bitcount.h

MICROCORE_OBJ	= $(MICROCORE_SRC:db:+$(o))
CORE_OBJ	= $(MICROCORE_OBJ) $(EXTRACORE_SRC:db:+$(o))
WIN32_OBJ	= $(WIN32_SRC:db:+$(o))
MINICORE_OBJ	= $(MINIDIR)\{$(MICROCORE_OBJ:f) miniperlmain$(o) perlio$(o)}
MINIWIN32_OBJ	= $(MINIDIR)\{$(WIN32_OBJ:f)}
MINI_OBJ	= $(MINICORE_OBJ) $(MINIWIN32_OBJ)
DLL_OBJ		= $(DYNALOADER)
X2P_OBJ		= $(X2P_SRC:db:+$(o))
GENUUDMAP_OBJ	= $(GENUUDMAP:db:+$(o))

PERLDLL_OBJ	= $(CORE_OBJ)
PERLEXE_OBJ	= perlmain$(o)
PERLEXEST_OBJ	= perlmainst$(o)

PERLDLL_OBJ	+= $(WIN32_OBJ) $(DLL_OBJ)

.IF "$(USE_SETARGV)" != ""
SETARGV_OBJ	= setargv$(o)
.ENDIF

.IF "$(ALL_STATIC)" == "define"
# some exclusions, unfortunately, until fixed:
#  - Win32 extension contains overlapped symbols with win32.c (BUG!)
#  - MakeMaker isn't capable enough for SDBM_File (smaller bug)
#  - Encode (encoding search algorithm relies on shared library?)
STATIC_EXT	= * !Win32 !SDBM_File !Encode
.ELSE
# specify static extensions here, for example:
#STATIC_EXT	= Cwd Compress/Raw/Zlib
STATIC_EXT	= Win32CORE
.ENDIF

DYNALOADER	= ..\DynaLoader$(o)

# vars must be separated by "\t+~\t+", since we're using the tempfile
# version of config_sh.pl (we were overflowing someone's buffer by
# trying to fit them all on the command line)
#	-- BKS 10-17-1999
CFG_VARS	=					\
		INST_DRV=$(INST_DRV)		~	\
		INST_TOP=$(INST_TOP)	~	\
		INST_VER=$(INST_VER)	~	\
		INST_ARCH=$(INST_ARCH)		~	\
		archname=$(ARCHNAME)		~	\
		cc=$(CC)			~	\
		ld=$(LINK32)			~	\
		ccflags=$(EXTRACFLAGS) $(OPTIMIZE) $(DEFINES) $(BUILDOPT)	~	\
		cf_email=$(EMAIL)		~	\
		d_crypt=$(D_CRYPT)		~	\
		d_mymalloc=$(PERL_MALLOC)	~	\
		libs=$(LIBFILES:f)		~	\
		incpath=$(CCINCDIR)	~	\
		libperl=$(PERLIMPLIB:f)		~	\
		libpth=$(CCLIBDIR);$(EXTRALIBDIRS)	~	\
		libc=$(LIBC)			~	\
		make=dmake			~	\
		_o=$(o)				~	\
		obj_ext=$(o)			~	\
		_a=$(a)				~	\
		lib_ext=$(a)			~	\
		static_ext=$(STATIC_EXT)	~	\
		usethreads=$(USE_ITHREADS)	~	\
		useithreads=$(USE_ITHREADS)	~	\
		usemultiplicity=$(USE_MULTI)	~	\
		useperlio=$(USE_PERLIO)		~	\
		uselargefiles=$(USE_LARGE_FILES)	~	\
		usesitecustomize=$(USE_SITECUST)	~	\
		LINK_FLAGS=$(LINK_FLAGS)	~	\
		optimize=$(OPTIMIZE)

#
# set up targets varying between Win95 and WinNT builds
#

.IF "$(IS_WIN95)" == "define"
MK2 		= .\makefile.95
RIGHTMAKE	= __switch_makefiles
.ELSE
MK2		= __not_needed
RIGHTMAKE	=
.ENDIF

.IMPORT .IGNORE : SystemRoot windir

# Don't just .IMPORT OS from the environment because dmake sets OS itself.
ENV_OS=$(subst,OS=, $(shell @set OS))

.IF "$(ENV_OS)" == "Windows_NT"
ODBCCP32_DLL = $(SystemRoot)\system32\odbccp32.dll
.ELSE
ODBCCP32_DLL = $(windir)\system\odbccp32.dll
.ENDIF

ICWD = -I..\cpan\Cwd -I..\cpan\Cwd\lib

#
# Top targets
#

all : CHECKDMAKE .\config.h ..\git_version.h $(GLOBEXE) $(MINIPERL) $(MK2)	\
	$(RIGHTMAKE) $(MINIMOD) $(CONFIGPM) $(UNIDATAFILES) MakePPPort		\
	$(PERLEXE) $(X2P) Extensions Extensions_nonxs $(PERLSTATIC)

regnodes : ..\regnodes.h

..\regcomp$(o) : ..\regnodes.h ..\regcharclass.h	

..\regexec$(o) : ..\regnodes.h ..\regcharclass.h

reonly : regnodes .\config.h ..\git_version.h $(GLOBEXE) $(MINIPERL) $(MK2)	\
	$(RIGHTMAKE) $(MINIMOD) $(CONFIGPM) $(UNIDATAFILES) $(PERLEXE)		\
	$(X2P) Extensions_reonly

static: $(PERLEXESTATIC)

#----------------------------------------------------------------

#-------------------- BEGIN Win95 SPECIFIC ----------------------

# this target is a jump-off point for Win95
#  1. it switches to the Win95-specific makefile if it exists
#     (__do_switch_makefiles)
#  2. it prints a message when the Win95-specific one finishes (__done)
#  3. it then kills this makefile by trying to make __no_such_target

__switch_makefiles: __do_switch_makefiles __done __no_such_target

__do_switch_makefiles:
.IF "$(NOTFIRST)" != "true"
	if exist $(MK2) $(MAKE:s/-S//) -f $(MK2) $(MAKETARGETS) NOTFIRST=true
.ELSE
	$(NOOP)
.ENDIF

.IF "$(NOTFIRST)" != "true"
__done:
	@echo Build process complete. Ignore any errors after this message.
	@echo Run "dmake test" to test and "dmake install" to install

.ELSE
# dummy targets for Win95-specific makefile

__done:
	$(NOOP)

__no_such_target:
	$(NOOP)

.ENDIF

# This target is used to generate the new makefile (.\makefile.95) for Win95

.\makefile.95: .\makefile.mk
	$(MINIPERL) genmk95.pl makefile.mk $(MK2)

#--------------------- END Win95 SPECIFIC ---------------------

# a blank target for when builds don't need to do certain things
# this target added for Win95 port but used to keep the WinNT port able to
# use this file
__not_needed:
	$(NOOP)

CHECKDMAKE :
.IF "$(NEWDMAKE)" == "define"
	$(NOOP)
.ELSE
	@echo Your dmake doesn't support ^|^| or ^&^& in conditional expressions.
	@echo Please get the latest dmake from http://search.cpan.org/dist/dmake/
	@exit 1
.ENDIF

$(GLOBEXE) : perlglob$(o)
.IF "$(CCTYPE)" == "BORLAND"
	$(CC) -c -w -v -tWM -I"$(CCINCDIR)" perlglob.c
	$(LINK32) -Tpe -ap $(BLINK_FLAGS) c0x32$(o) perlglob$(o) \
	    "$(CCLIBDIR)\32BIT\wildargs$(o)",$@,,import32.lib cw32mt.lib,
.ELIF "$(CCTYPE)" == "GCC"
	$(LINK32) $(BLINK_FLAGS) -mconsole -o $@ perlglob$(o) $(LIBFILES)
.ELSE
	$(LINK32) $(BLINK_FLAGS) $(LIBFILES) -out:$@ -subsystem:$(SUBSYS) \
	    perlglob$(o) setargv$(o)
	$(EMBED_EXE_MANI)
.ENDIF

perlglob$(o)  : perlglob.c

config.w32 : $(CFGSH_TMPL)
	copy $(CFGSH_TMPL) config.w32

.\config.h : $(CFGH_TMPL) $(CORE_NOCFG_H)
	-del /f config.h
	copy $(CFGH_TMPL) config.h

..\git_version.h : $(MINIPERL) ..\make_patchnum.pl
	cd .. && miniperl -Ilib make_patchnum.pl

# make sure that we recompile perl.c if the git version changes
..\perl$(o) : ..\git_version.h

..\config.sh : config.w32 $(MINIPERL) config_sh.PL FindExt.pm
	$(MINIPERL) -I..\lib config_sh.PL --cfgsh-option-file \
	    $(mktmp $(CFG_VARS)) config.w32 > ..\config.sh

# this target is for when changes to the main config.sh happen.
# edit config.gc, then make perl using GCC in a minimal configuration (i.e.
# with MULTI, ITHREADS, IMP_SYS, LARGE_FILES, PERLIO and CRYPT off), then make
# this target to regenerate config_H.gc.
# unfortunately, some further manual editing is also then required to restore all
# the special _MSC_VER handling that is otherwise lost.
# repeat for config.bc and config_H.bc (using BORLAND), except that there is no
# _MSC_VER stuff in that case.
regen_config_h:
	$(MINIPERL) -I..\lib config_sh.PL --cfgsh-option-file $(mktmp $(CFG_VARS)) \
	    $(CFGSH_TMPL) > ..\config.sh
	$(MINIPERL) -I..\lib ..\configpm --chdir=..
	-del /f $(CFGH_TMPL)
	-$(MINIPERL) -I..\lib $(ICWD) config_h.PL "INST_VER=$(INST_VER)"
	rename config.h $(CFGH_TMPL)

$(CONFIGPM) : $(MINIPERL) ..\config.sh config_h.PL ..\minimod.pl
	$(MINIPERL) -I..\lib ..\configpm --chdir=..
	if exist lib\* $(RCOPY) lib\*.* ..\lib\$(NULL)
	$(XCOPY) ..\*.h $(COREDIR)\*.*
	$(XCOPY) *.h $(COREDIR)\*.*
	$(XCOPY) ..\ext\re\re.pm $(LIBDIR)\*.*
	$(RCOPY) include $(COREDIR)\*.*
	$(MINIPERL) -I..\lib $(ICWD) config_h.PL "INST_VER=$(INST_VER)" \
	    || $(MAKE) $(MAKEMACROS) $(CONFIGPM) $(MAKEFILE)

$(MINIPERL) : $(MINIDIR) $(MINI_OBJ) $(CRTIPMLIBS)
.IF "$(CCTYPE)" == "BORLAND"
	if not exist $(CCLIBDIR)\PSDK\odbccp32.lib \
	    cd $(CCLIBDIR)\PSDK && implib odbccp32.lib $(ODBCCP32_DLL)
	$(LINK32) -Tpe -ap $(BLINK_FLAGS) \
	    @$(mktmp c0x32$(o) $(MINI_OBJ),$@,,$(LIBFILES),)
.ELIF "$(CCTYPE)" == "GCC"
	$(LINK32) -v -mconsole -o $@ $(BLINK_FLAGS) \
	    $(mktmp $(LKPRE) $(MINI_OBJ) $(LIBFILES) $(LKPOST))
.ELSE
	$(LINK32) -subsystem:console -out:$@ $(BLINK_FLAGS) \
	    @$(mktmp $(LIBFILES) $(MINI_OBJ))
	$(EMBED_EXE_MANI)
.ENDIF

$(MINIDIR) :
	if not exist "$(MINIDIR)" mkdir "$(MINIDIR)"

$(MINICORE_OBJ) : $(CORE_NOCFG_H)
	$(CC) -c $(CFLAGS) -DPERL_EXTERNAL_GLOB -DPERL_IS_MINIPERL $(OBJOUT_FLAG)$@ ..\$(*B).c

$(MINIWIN32_OBJ) : $(CORE_NOCFG_H)
	$(CC) -c $(CFLAGS) $(OBJOUT_FLAG)$@ $(*B).c

# -DPERL_IMPLICIT_SYS needs C++ for perllib.c
# rules wrapped in .IFs break Win9X build (we end up with unbalanced []s unless
# unless the .IF is true), so instead we use a .ELSE with the default.
# This is the only file that depends on perlhost.h, vmem.h, and vdir.h

perllib$(o)	: perllib.c .\perlhost.h .\vdir.h .\vmem.h
.IF "$(USE_IMP_SYS)" == "define"
	$(CC) -c -I. $(CFLAGS_O) $(CXX_FLAG) $(OBJOUT_FLAG)$@ perllib.c
.ELSE
	$(CC) -c -I. $(CFLAGS_O) $(OBJOUT_FLAG)$@ perllib.c
.ENDIF

# 1. we don't want to rebuild miniperl.exe when config.h changes
# 2. we don't want to rebuild miniperl.exe with non-default config.h
# 3. we can't have miniperl.exe depend on git_version.h, as miniperl creates it
$(MINI_OBJ)	: $(CORE_NOCFG_H)

$(WIN32_OBJ)	: $(CORE_H)

$(CORE_OBJ)	: $(CORE_H)

$(DLL_OBJ)	: $(CORE_H)

$(X2P_OBJ)	: $(CORE_H)

perldll.def : $(MINIPERL) $(CONFIGPM) ..\global.sym ..\pp.sym ..\makedef.pl create_perllibst_h.pl
	$(MINIPERL) -I..\lib create_perllibst_h.pl
	$(MINIPERL) -I..\lib -w ..\makedef.pl PLATFORM=win32 $(OPTIMIZE) $(DEFINES) \
	$(BUILDOPT) CCTYPE=$(CCTYPE) > perldll.def

$(PERLDLL): perldll.def $(PERLDLL_OBJ) $(PERLDLL_RES) Extensions_static
.IF "$(CCTYPE)" == "BORLAND"
	$(LINK32) -Tpd -ap $(BLINK_FLAGS) \
	    @$(mktmp c0d32$(o) $(PERLDLL_OBJ),$@,, \
	        $(shell @type Extensions_static) $(LIBFILES),perldll.def)
	$(IMPLIB) $*.lib $@
.ELIF "$(CCTYPE)" == "GCC"
	$(LINK32) -mdll -o $@ -Wl,--base-file -Wl,perl.base $(BLINK_FLAGS) \
	    $(mktmp $(LKPRE) $(PERLDLL_OBJ) \
		$(shell @type Extensions_static) \
		$(LIBFILES) $(LKPOST))
	$(IMPLIB) --output-lib $(PERLIMPLIB) \
		--dllname $(PERLDLL:b).dll \
		--def perldll.def \
		--base-file perl.base \
		--output-exp perl.exp
	$(LINK32) -mdll -o $@ $(BLINK_FLAGS) \
	    $(mktmp $(LKPRE) $(PERLDLL_OBJ) \
		$(shell @type Extensions_static) \
		$(LIBFILES) perl.exp $(LKPOST))
.ELSE
	$(LINK32) -dll -def:perldll.def -out:$@ $(BLINK_FLAGS) \
	    @Extensions_static \
	    @$(mktmp -base:0x28000000 $(DELAYLOAD) $(LIBFILES) \
		$(PERLDLL_RES) $(PERLDLL_OBJ))
	$(EMBED_DLL_MANI)
.ENDIF
	$(XCOPY) $(PERLIMPLIB) $(COREDIR)

$(PERLSTATICLIB): Extensions_static
.IF "$(CCTYPE)" == "BORLAND"
	$(LIB32) $(LIB_FLAGS) $@ \
	    @$(mktmp $(shell @type Extensions_static) \
		$(PERLDLL_OBJ))
.ELIF "$(CCTYPE)" == "GCC"
# XXX: It would be nice if MinGW's ar accepted a temporary file, but this
# doesn't seem to work:
#	$(LIB32) $(LIB_FLAGS) $@ \
#	    $(mktmp $(LKPRE) $(shell @type Extensions_static) \
#		$(PERLDLL_OBJ) $(LKPOST))
	$(LIB32) $(LIB_FLAGS) $@ \
	    $(shell @type Extensions_static) \
	    $(PERLDLL_OBJ)
.ELSE
	$(LIB32) $(LIB_FLAGS) -out:$@ @Extensions_static \
	    @$(mktmp $(PERLDLL_OBJ))
.ENDIF
	$(XCOPY) $(PERLSTATICLIB) $(COREDIR)

$(PERLEXE_RES): perlexe.rc $(PERLEXE_MANIFEST) $(PERLEXE_ICO)

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

$(X2P) : $(MINIPERL) $(X2P_OBJ) Extensions
	$(MINIPERL) -I..\lib ..\x2p\find2perl.PL
	$(MINIPERL) -I..\lib ..\x2p\s2p.PL
.IF "$(CCTYPE)" == "BORLAND"
	$(LINK32) -Tpe -ap $(BLINK_FLAGS) \
	    @$(mktmp c0x32$(o) $(X2P_OBJ),$@,,$(LIBFILES),)
.ELIF "$(CCTYPE)" == "GCC"
	$(LINK32) -v -o $@ $(BLINK_FLAGS) \
	    $(mktmp $(LKPRE) $(X2P_OBJ) $(LIBFILES) $(LKPOST))
.ELSE
	$(LINK32) -subsystem:console -out:$@ $(BLINK_FLAGS) \
	    @$(mktmp $(LIBFILES) $(X2P_OBJ))
	$(EMBED_EXE_MANI)
.ENDIF

$(MINIDIR)\globals$(o) : $(UUDMAP_H) $(BITCOUNT_H)

$(UUDMAP_H) $(BITCOUNT_H) : $(GENUUDMAP)
	$(GENUUDMAP) $(UUDMAP_H) $(BITCOUNT_H)

$(GENUUDMAP) : $(GENUUDMAP_OBJ)
.IF "$(CCTYPE)" == "BORLAND"
	$(LINK32) -Tpe -ap $(BLINK_FLAGS) \
	    @$(mktmp c0x32$(o) $(GENUUDMAP_OBJ),$@,,$(LIBFILES),)
.ELIF "$(CCTYPE)" == "GCC"
	$(LINK32) -v -o $@ $(BLINK_FLAGS) \
	    $(mktmp $(LKPRE) $(GENUUDMAP_OBJ) $(LIBFILES) $(LKPOST))
.ELSE
	$(LINK32) -subsystem:console -out:$@ $(BLINK_FLAGS) \
	    @$(mktmp $(LIBFILES) $(GENUUDMAP_OBJ))
	$(EMBED_EXE_MANI)
.ENDIF

perlmain.c : runperl.c
	copy runperl.c perlmain.c

perlmain$(o) : perlmain.c
	$(CC) $(CFLAGS_O:s,-DPERLDLL,-UPERLDLL,) $(OBJOUT_FLAG)$@ -c perlmain.c

perlmainst.c : runperl.c
	copy runperl.c perlmainst.c

perlmainst$(o) : perlmainst.c
	$(CC) $(CFLAGS_O) $(OBJOUT_FLAG)$@ -c perlmainst.c

$(PERLEXE): $(PERLDLL) $(CONFIGPM) $(PERLEXE_OBJ) $(PERLEXE_RES)
.IF "$(CCTYPE)" == "BORLAND"
	$(LINK32) -Tpe -ap $(BLINK_FLAGS) \
	    @$(mktmp c0x32$(o) $(PERLEXE_OBJ),$@,, \
		$(PERLIMPLIB) $(LIBFILES),,$(PERLEXE_RES))
.ELIF "$(CCTYPE)" == "GCC"
	$(LINK32) -mconsole -o $@ $(BLINK_FLAGS)  \
	    $(PERLEXE_OBJ) $(PERLEXE_RES) $(PERLIMPLIB) $(LIBFILES)
.ELSE
	$(LINK32) -subsystem:console -out:$@ -stack:0x1000000 $(BLINK_FLAGS) \
	    $(LIBFILES) $(PERLEXE_OBJ) $(SETARGV_OBJ) $(PERLIMPLIB) $(PERLEXE_RES)
	$(EMBED_EXE_MANI)
.ENDIF
	copy $(PERLEXE) $(WPERLEXE)
	$(MINIPERL) -I..\lib bin\exetype.pl $(WPERLEXE) WINDOWS

$(PERLEXESTATIC): $(PERLSTATICLIB) $(CONFIGPM) $(PERLEXEST_OBJ) $(PERLEXE_RES)
.IF "$(CCTYPE)" == "BORLAND"
	$(LINK32) -Tpe -ap $(BLINK_FLAGS) \
	    @$(mktmp c0x32$(o) $(PERLEXEST_OBJ),$@,, \
		$(shell @type Extensions_static) $(PERLSTATICLIB) $(LIBFILES),, \
		$(PERLEXE_RES))
.ELIF "$(CCTYPE)" == "GCC"
	$(LINK32) -mconsole -o $@ $(BLINK_FLAGS) \
	    $(mktmp $(LKPRE) $(shell @type Extensions_static) \
		$(PERLSTATICLIB) $(LIBFILES) $(PERLEXEST_OBJ) \
		$(PERLEXE_RES) $(LKPOST))
.ELSE
	$(LINK32) -subsystem:console -out:$@ -stack:0x1000000 $(BLINK_FLAGS) \
	    @Extensions_static $(PERLSTATICLIB) /PDB:NONE \
	    $(LIBFILES) $(PERLEXEST_OBJ) $(SETARGV_OBJ) $(PERLEXE_RES)
	$(EMBED_EXE_MANI)
.ENDIF

MakePPPort: $(MINIPERL) $(CONFIGPM) Extensions_nonxs
	$(MINIPERL) -I..\lib $(ICWD) ..\mkppport

#-------------------------------------------------------------------------------
# There's no direct way to mark a dependency on
# DynaLoader.pm, so this will have to do
Extensions : ..\make_ext.pl $(PERLDEP) $(CONFIGPM) $(DYNALOADER)
	$(XCOPY) ..\*.h $(COREDIR)\*.*
	$(MINIPERL) -I..\lib ..\make_ext.pl "MAKE=$(MAKE)" --dir=$(CPANDIR) --dir=$(DISTDIR) --dir=$(EXTDIR) --dynamic

Extensions_reonly : ..\make_ext.pl $(PERLDEP) $(CONFIGPM) $(DYNALOADER)
	$(XCOPY) ..\*.h $(COREDIR)\*.*
	$(MINIPERL) -I..\lib ..\make_ext.pl "MAKE=$(MAKE)" --dir=$(CPANDIR) --dir=$(DISTDIR) --dir=$(EXTDIR) --dynamic +re

Extensions_static : ..\make_ext.pl list_static_libs.pl $(PERLDEP) $(CONFIGPM)
	$(XCOPY) ..\*.h $(COREDIR)\*.*
	$(MINIPERL) -I..\lib ..\make_ext.pl "MAKE=$(MAKE)" --dir=$(CPANDIR) --dir=$(DISTDIR) --dir=$(EXTDIR) --static
	$(MINIPERL) -I..\lib list_static_libs.pl > Extensions_static

Extensions_nonxs : ..\make_ext.pl $(PERLDEP) $(CONFIGPM)
	$(XCOPY) ..\*.h $(COREDIR)\*.*
	$(MINIPERL) -I..\lib ..\make_ext.pl "MAKE=$(MAKE)" --dir=$(CPANDIR) --dir=$(DISTDIR) --dir=$(EXTDIR) --nonxs

$(DYNALOADER) : ..\make_ext.pl $(PERLDEP) $(CONFIGPM) Extensions_nonxs
	$(XCOPY) ..\*.h $(COREDIR)\*.*
	$(MINIPERL) -I..\lib ..\make_ext.pl "MAKE=$(MAKE)" --dir=$(EXTDIR) --dynaloader

Extensions_clean :
	-if exist $(MINIPERL) $(MINIPERL) -I..\lib ..\make_ext.pl "MAKE=$(MAKE)" --dir=$(CPANDIR) --dir=$(DISTDIR) --dir=$(EXTDIR) --all --target=clean

Extensions_realclean :
	-if exist $(MINIPERL) $(MINIPERL) -I..\lib ..\make_ext.pl "MAKE=$(MAKE)" --dir=$(CPANDIR) --dir=$(DISTDIR) --dir=$(EXTDIR) --all --target=realclean

#-------------------------------------------------------------------------------


doc: $(PERLEXE) ..\pod\perltoc.pod
	$(PERLEXE) -I..\lib ..\installhtml --podroot=.. --htmldir=$(HTMLDIR) \
	    --podpath=pod:lib:ext:utils --htmlroot="file://$(INST_HTML:s,:,|,)"\
	    --libpod=perlfunc:perlguts:perlvar:perlrun:perlop --recurse

# Note that this next section is parsed (and regenerated) by pod/buildtoc
# so please check that script before making structural changes here
utils: $(PERLEXE) $(X2P)
	cd ..\utils && $(MAKE) PERL=$(MINIPERL)
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
	copy ..\README.haiku    ..\pod\perlhaiku.pod
	copy ..\README.hpux     ..\pod\perlhpux.pod
	copy ..\README.hurd     ..\pod\perlhurd.pod
	copy ..\README.irix     ..\pod\perlirix.pod
	copy ..\README.jp       ..\pod\perljp.pod
	copy ..\README.ko       ..\pod\perlko.pod
	copy ..\README.linux    ..\pod\perllinux.pod
	copy ..\README.macos    ..\pod\perlmacos.pod
	copy ..\README.macosx   ..\pod\perlmacosx.pod
	copy ..\README.mpeix    ..\pod\perlmpeix.pod
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
	copy ..\README.tru64    ..\pod\perltru64.pod
	copy ..\README.tw       ..\pod\perltw.pod
	copy ..\README.uts      ..\pod\perluts.pod
	copy ..\README.vmesa    ..\pod\perlvmesa.pod
	copy ..\README.vos      ..\pod\perlvos.pod
	copy ..\README.win32    ..\pod\perlwin32.pod
	copy ..\pod\perl5122delta.pod ..\pod\perldelta.pod
	cd ..\pod && $(MAKE) -f ..\win32\pod.mak converters
	$(PERLEXE) $(PL2BAT) $(UTILS)
	$(PERLEXE) $(ICWD) ..\autodoc.pl ..
	$(PERLEXE) $(ICWD) ..\pod\perlmodlib.pl -q

..\pod\perltoc.pod: $(PERLEXE) Extensions Extensions_nonxs
	$(PERLEXE) -f ..\pod\buildtoc --build-toc -q

# Note that the pod cleanup in this next section is parsed (and regenerated
# by pod/buildtoc so please check that script before making changes here

distclean: realclean
	-del /f $(MINIPERL) $(PERLEXE) $(PERLDLL) $(GLOBEXE) \
		$(PERLIMPLIB) ..\miniperl$(a) $(MINIMOD) \
		$(PERLEXESTATIC) $(PERLSTATICLIB)
	-del /f *.def *.map
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
	-del /f $(DISTDIR)\XSLoader\XSLoader.pm
	-if exist $(LIBDIR)\App rmdir /s /q $(LIBDIR)\App
	-if exist $(LIBDIR)\Archive rmdir /s /q $(LIBDIR)\Archive
	-if exist $(LIBDIR)\Attribute rmdir /s /q $(LIBDIR)\Attribute
	-if exist $(LIBDIR)\autodie rmdir /s /q $(LIBDIR)\autodie
	-if exist $(LIBDIR)\B rmdir /s /q $(LIBDIR)\B
	-if exist $(LIBDIR)\CGI rmdir /s /q $(LIBDIR)\CGI
	-if exist $(LIBDIR)\CPAN rmdir /s /q $(LIBDIR)\CPAN
	-if exist $(LIBDIR)\CPANPLUS rmdir /s /q $(LIBDIR)\CPANPLUS
	-if exist $(LIBDIR)\Compress rmdir /s /q $(LIBDIR)\Compress
	-if exist $(LIBDIR)\Data rmdir /s /q $(LIBDIR)\Data
	-if exist $(LIBDIR)\Devel rmdir /s /q $(LIBDIR)\Devel
	-if exist $(LIBDIR)\Digest rmdir /s /q $(LIBDIR)\Digest
	-if exist $(LIBDIR)\Encode rmdir /s /q $(LIBDIR)\Encode
	-if exist $(LIBDIR)\encoding rmdir /s /q $(LIBDIR)\encoding
	-if exist $(LIBDIR)\ExtUtils\CBuilder rmdir /s /q $(LIBDIR)\ExtUtils\CBuilder
	-if exist $(LIBDIR)\ExtUtils\Command rmdir /s /q $(LIBDIR)\ExtUtils\Command
	-if exist $(LIBDIR)\ExtUtils\Constant rmdir /s /q $(LIBDIR)\ExtUtils\Constant
	-if exist $(LIBDIR)\ExtUtils\Liblist rmdir /s /q $(LIBDIR)\ExtUtils\Liblist
	-if exist $(LIBDIR)\ExtUtils\MakeMaker rmdir /s /q $(LIBDIR)\ExtUtils\MakeMaker
	-if exist $(LIBDIR)\File\Spec rmdir /s /q $(LIBDIR)\File\Spec
	-if exist $(LIBDIR)\Filter rmdir /s /q $(LIBDIR)\Filter
	-if exist $(LIBDIR)\Hash rmdir /s /q $(LIBDIR)\Hash
	-if exist $(LIBDIR)\I18N\LangTags rmdir /s /q $(LIBDIR)\I18N\LangTags
	-if exist $(LIBDIR)\inc rmdir /s /q $(LIBDIR)\inc
	-if exist $(LIBDIR)\Module\Pluggable rmdir /s /q $(LIBDIR)\Module\Pluggable
	-if exist $(LIBDIR)\IO rmdir /s /q $(LIBDIR)\IO
	-if exist $(LIBDIR)\IPC rmdir /s /q $(LIBDIR)\IPC
	-if exist $(LIBDIR)\List rmdir /s /q $(LIBDIR)\List
	-if exist $(LIBDIR)\Locale rmdir /s /q $(LIBDIR)\Locale
	-if exist $(LIBDIR)\Log rmdir /s /q $(LIBDIR)\Log
	-if exist $(LIBDIR)\Math rmdir /s /q $(LIBDIR)\Math
	-if exist $(LIBDIR)\Memoize rmdir /s /q $(LIBDIR)\Memoize
	-if exist $(LIBDIR)\MIME rmdir /s /q $(LIBDIR)\MIME
	-if exist $(LIBDIR)\Module rmdir /s /q $(LIBDIR)\Module
	-if exist $(LIBDIR)\mro rmdir /s /q $(LIBDIR)\mro
	-if exist $(LIBDIR)\Net\FTP rmdir /s /q $(LIBDIR)\Net\FTP
	-if exist $(LIBDIR)\Object rmdir /s /q $(LIBDIR)\Object
	-if exist $(LIBDIR)\Package rmdir /s /q $(LIBDIR)\Package
	-if exist $(LIBDIR)\Params rmdir /s /q $(LIBDIR)\Params
	-if exist $(LIBDIR)\Parse rmdir /s /q $(LIBDIR)\Parse
	-if exist $(LIBDIR)\PerlIO rmdir /s /q $(LIBDIR)\PerlIO
	-if exist $(LIBDIR)\Pod\Perldoc rmdir /s /q $(LIBDIR)\Pod\Perldoc
	-if exist $(LIBDIR)\Pod\Simple rmdir /s /q $(LIBDIR)\Pod\Simple
	-if exist $(LIBDIR)\Pod\Text rmdir /s /q $(LIBDIR)\Pod\Text
	-if exist $(LIBDIR)\re rmdir /s /q $(LIBDIR)\re
	-if exist $(LIBDIR)\Scalar rmdir /s /q $(LIBDIR)\Scalar
	-if exist $(LIBDIR)\Sys rmdir /s /q $(LIBDIR)\Sys
	-if exist $(LIBDIR)\TAP rmdir /s /q $(LIBDIR)\TAP
	-if exist $(LIBDIR)\Term\UI rmdir /s /q $(LIBDIR)\Term\UI
	-if exist $(LIBDIR)\Test rmdir /s /q $(LIBDIR)\Test
	-if exist $(LIBDIR)\Thread rmdir /s /q $(LIBDIR)\Thread
	-if exist $(LIBDIR)\threads rmdir /s /q $(LIBDIR)\threads
	-if exist $(LIBDIR)\Unicode\Collate rmdir /s /q $(LIBDIR)\Unicode\Collate
	-if exist $(LIBDIR)\XS rmdir /s /q $(LIBDIR)\XS
	-if exist $(LIBDIR)\Win32API rmdir /s /q $(LIBDIR)\Win32API
	-cd $(PODDIR) && del /f *.html *.bat \
	    perlaix.pod perlamiga.pod perlapi.pod perlapollo.pod \
	    perlbeos.pod perlbs2000.pod perlce.pod perlcn.pod \
	    perlcygwin.pod perldelta.pod perldgux.pod perldos.pod \
	    perlepoc.pod perlfreebsd.pod perlhaiku.pod perlhpux.pod \
	    perlhurd.pod perlintern.pod perlirix.pod perljp.pod perlko.pod \
	    perllinux.pod perlmacos.pod perlmacosx.pod perlmodlib.pod \
	    perlmpeix.pod perlnetware.pod perlopenbsd.pod perlos2.pod \
	    perlos390.pod perlos400.pod perlplan9.pod perlqnx.pod \
	    perlriscos.pod perlsolaris.pod perlsymbian.pod perltoc.pod \
	    perltru64.pod perltw.pod perluniprops.pod perluts.pod \
	    perlvmesa.pod perlvos.pod perlwin32.pod \
	    pod2html pod2latex pod2man pod2text pod2usage \
	    podchecker podselect
	-cd ..\utils && del /f h2ph splain perlbug pl2pm c2ph pstruct h2xs \
	    perldoc perlivp dprofpp libnetcfg enc2xs piconv cpan *.bat \
	    xsubpp instmodsh prove ptar ptardiff cpanp-run-perl cpanp cpan2dist shasum corelist config_data
	-cd ..\x2p && del /f find2perl s2p psed *.bat
	-del /f ..\config.sh perlmain.c dlutils.c config.h.new \
	    perlmainst.c
	-del /f $(CONFIGPM)
	-del /f ..\lib\Config_git.pl
	-del /f bin\*.bat
	-del /f perllibst.h
	-del /f $(PERLEXE_RES) perl.base
	-cd .. && del /s *$(a) *.map *.pdb *.ilk *.tds *.bs *$(o) .exists pm_to_blib ppport.h
	-cd $(EXTDIR) && del /s *.def Makefile Makefile.old
	-cd $(DISTDIR) && del /s *.def Makefile Makefile.old
	-cd $(CPANDIR) && del /s *.def Makefile Makefile.old
	-if exist $(AUTODIR) rmdir /s /q $(AUTODIR)
	-if exist $(COREDIR) rmdir /s /q $(COREDIR)
	-if exist pod2htmd.tmp del pod2htmd.tmp
	-if exist pod2htmi.tmp del pod2htmi.tmp
	-if exist $(HTMLDIR) rmdir /s /q $(HTMLDIR)
	-del /f ..\t\test_state

install : all installbare installhtml

installbare : $(RIGHTMAKE) utils ..\pod\perltoc.pod
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

$(UNIDATAFILES) ..\pod\perluniprops.pod .UPDATEALL : $(MINIPERL) $(CONFIGPM) ..\lib\unicore\mktables Extensions_nonxs
	cd ..\lib\unicore && \
	..\$(MINIPERL) -I.. -I..\..\cpan\Cwd\lib -I..\..\cpan\Cwd mktables -P ..\..\pod -maketest -makelist -p

minitest : $(MINIPERL) $(GLOBEXE) $(CONFIGPM) $(UNIDATAFILES) utils
	$(XCOPY) $(MINIPERL) ..\t\$(NULL)
	if exist ..\t\perl.exe del /f ..\t\perl.exe
	rename ..\t\miniperl.exe perl.exe
.IF "$(CCTYPE)" == "BORLAND"
	$(XCOPY) $(GLOBBAT) ..\t\$(NULL)
.ELSE
	$(XCOPY) $(GLOBEXE) ..\t\$(NULL)
.ENDIF
	attrib -r ..\t\*.*
	cd ..\t && \
	$(MINIPERL) -I..\lib harness base/*.t comp/*.t cmd/*.t io/*.t op/*.t pragma/*.t

test-prep : all utils
	$(XCOPY) $(PERLEXE) ..\t\$(NULL)
	$(XCOPY) $(PERLDLL) ..\t\$(NULL)
.IF "$(CCTYPE)" == "BORLAND"
	$(XCOPY) $(GLOBBAT) ..\t\$(NULL)
.ELSE
	$(XCOPY) $(GLOBEXE) ..\t\$(NULL)
.ENDIF
.IF "$(CCTYPE)" == "GCC"
.IF "$(GCC_4XX)" == "define"
	$(XCOPY) $(GCCHELPERDLL) ..\t\$(NULL)
.ENDIF
.ENDIF

test : $(RIGHTMAKE) test-prep
	cd ..\t && $(PERLEXE) -I..\lib harness $(TEST_SWITCHES) $(TEST_FILES)

test-reonly : reonly utils
	$(XCOPY) $(PERLEXE) ..\t\$(NULL)
	$(XCOPY) $(PERLDLL) ..\t\$(NULL)
	$(XCOPY) $(GLOBEXE) ..\t\$(NULL)
	cd ..\t && \
	$(PERLEXE) -I..\lib harness $(OPT) -re \bpat\\/ $(EXTRA) && \
	cd ..\win32

regen :
	cd .. && regen.pl && cd win32

test-notty : test-prep
	set PERL_SKIP_TTY_TEST=1 && \
	    cd ..\t && $(PERLEXE) -I.\lib harness $(TEST_SWITCHES) $(TEST_FILES)

_test : $(RIGHTMAKE)
	$(XCOPY) $(PERLEXE) ..\t\$(NULL)
	$(XCOPY) $(PERLDLL) ..\t\$(NULL)
.IF "$(CCTYPE)" == "BORLAND"
	$(XCOPY) $(GLOBBAT) ..\t\$(NULL)
.ELSE
	$(XCOPY) $(GLOBEXE) ..\t\$(NULL)
.ENDIF
	cd ..\t && $(PERLEXE) -I..\lib harness $(TEST_SWITCHES) $(TEST_FILES)

_clean :
	-@erase miniperlmain$(o)
	-@erase $(MINIPERL)
	-@erase perlglob$(o)
	-@erase perlmain$(o)
	-@erase perlmainst$(o)
	-@erase config.w32
	-@erase /f config.h
	-@erase /f ..\git_version.h
	-@erase $(GLOBEXE)
	-@erase $(PERLEXE)
	-@erase $(WPERLEXE)
	-@erase $(PERLEXESTATIC)
	-@erase $(PERLSTATICLIB)
	-@erase $(PERLDLL)
	-@erase $(CORE_OBJ)
	-@erase $(GENUUDMAP) $(GENUUDMAP_OBJ) $(UUDMAP_H) $(BITCOUNT_H)
	-if exist $(MINIDIR) rmdir /s /q $(MINIDIR)
	-if exist $(UNIDATADIR1) rmdir /s /q $(UNIDATADIR1)
	-if exist $(UNIDATADIR2) rmdir /s /q $(UNIDATADIR2)
	-@erase $(UNIDATAFILES)
	-@erase $(WIN32_OBJ)
	-@erase $(DLL_OBJ)
	-@erase $(X2P_OBJ)
	-@erase ..\*$(o) ..\*$(a) ..\*.exp *$(o) *$(a) *.exp *.res
	-@erase ..\t\*.exe ..\t\*.dll ..\t\*.bat
	-@erase ..\x2p\*.exe ..\x2p\*.bat
	-@erase *.ilk
	-@erase *.pdb
	-@erase *.tds
	-@erase Extensions_static



clean : Extensions_clean _clean

realclean : Extensions_realclean _clean

# Handy way to run perlbug -ok without having to install and run the
# installed perlbug. We don't re-run the tests here - we trust the user.
# Please *don't* use this unless all tests pass.
# If you want to report test failures, use "dmake nok" instead.
ok: utils
	$(PERLEXE) -I..\lib ..\utils\perlbug -ok -s "(UNINSTALLED)"

okfile: utils
	$(PERLEXE) -I..\lib ..\utils\perlbug -ok -s "(UNINSTALLED)" -F perl.ok

nok: utils
	$(PERLEXE) -I..\lib ..\utils\perlbug -nok -s "(UNINSTALLED)"

nokfile: utils
	$(PERLEXE) -I..\lib ..\utils\perlbug -nok -s "(UNINSTALLED)" -F perl.nok
