##
## Makefile to build Perl on NetWare using Microsoft NMAKE and CodeWarrior tools
##
## This will build perl.nlm, perl.lib and extensions called NLMs
##

##
## Please read README.netware before starting
##

##
## Build configuration.  Edit the values below to suit your needs.
##

## This file is created by using the makefile that creates Windows Perl as the reference
## Author:
## Date Created: 13th July 2000
## Date Modified: 21st March 2002
# Name of the NLM


NLM_NAME		= perl.nlm
NLM_NAME8		= Perl

MAKE_ACTION		= Build

# Flags
DBG_FLAG	= -DDEBUGON

NW_FLAGS	= -DNETWARE -DNLM_PLATFORM -DNETDB_USE_INTERNET

REL_DIR	= Release
DEB_DIR = Debug


!ifndef NLMSDKBASE
!message "Run bat\SetNWBld.bat to set the NetWare SDK before continuing.\n"
!error
!endif											# !ifndef NLMSDKBASE

!ifndef CODEWAR									# !ifdef CODEWAR
!message "CodeWarrior tools base directory is not defined. Run bat\setnwbld.bat before proceeding"
!error
!endif											# !ifdef CODEWAR

!ifndef MAKE_TYPE
!message "Run bat\buildtype.bat to set the build type before continuing.\n"
!error
!endif											# !ifndef MAKE_TYPE

!ifdef USE_MPK
MPKFLAGS	= -DMPK_ON -DIAPX386
MPKMESSAGE = MPK Build...
XDCTOOL	= mpkxdc
!ifndef MPKBASE
!message "Run bat\setmpksdk.bat to set the NetWare MPK SDK before continuing.\n"
!error
!endif											# !ifndef MPKBASE
NLM_INCLUDE_MP		= -I$(MPKBASE)\include
MPKTOOL = $(MPKBASE)\$(XDCTOOL)
!else											# !ifdef USE_MPK
MPKMESSAGE = Non MPK Build...
NLM_INCLUDE_MP =
MPKTOOL = 
!endif											# !ifdef USE_MPK

#!ifndef SECURITYBASE
#!message "Run bat\SetSecSdk.bat to set the Security path before continuing.\n"
#!error
#!endif											# !ifndef SECURITYBASE

#!ifndef UCSINC
#!message "Run bat\BldNWExt.bat to set the UCS Include path before continuing.\n"
#!error
#!endif											# !ifndef UCSINC

NLMIMPORTS			= $(NLMSDKBASE)\imports

!ifdef SECURITYBASE
SECURITY_IMPORTS	= $(SECURITYBASE)\imports
!endif											# !ifdef SECURITYBASE

!ifndef NLM_VERSION
NLM_VERSION    = 3,20,0
!endif


# Here comes the CW tools - TO BE FILLED TO BUILD WITH CW -
MODULE_DESC     = "Perl 5.8.3 for NetWare"
CCTYPE          = CodeWarrior
C_COMPILER		= mwccnlm -c
CPP_COMPILER	= mwccnlm
LINK		    = mwldnlm      
LD              = mwldnlm
NLM_LIB	    	= mwldnlm -type library
TOOL_HEADERS	= 
TOOL_PATH		= 
CWCPPFLAGS      = -cpp_exceptions on -wchar_t off -bool on -w on -ansi off
CCFLAGS			= -maxerrors 25 -processor Pentium -align packed \
                  -w nounusedarg -msext on \
                  -DN_PLAT_NLM -DNLM=1 -D__NO_MATH_OPS -msgstyle gcc
COMPILER_FLAG	= -d NETWARE
ERROR_FLAG		= -sym on -sym codeview4 -sym internal
LDFLAGS         = -type generic -stacksize 16384 -zerobss \
                  -nofail -msgstyle gcc -nostderr -w on \
                  -nlmversion $(NLM_VERSION) \
                  -copy "Copyright (C) 2000-01\, 2002 Novell\, Inc. All Rights Reserved."

# Debug flags comes here - Not mandatory - required only for debug build
!if "$(MAKE_TYPE)"=="Debug"
BLDDIR          = $(DEB_DIR)
BLDMESG	    = Debug version,
!ifdef USE_D2
BS_CFLAGS	= -opt off -inline off -sym on -sym codeview4 -sym internal -DDEBUGGING -DDKFBPON
BLDMESG		= $(BLDMESG) Using /d2 option
!ifdef NLM_NAME8
LDFLAGS		= $(LDFLAGS) -sym on -sym codeview4 -sym internal -osym $(MAKE_TYPE)\$(NLM_NAME8).sym
!else												# !ifdef NLM_NAME8
LDFLAGS		= $(LDFLAGS) -sym on -sym codeview4 -sym internal -osym $(MAKE_TYPE)\$(NLM_NAME).sym
!endif												# !ifdef NLM_NAME8
!else												# !ifdef USE_D2
BS_CFLAGS	= -opt off -inline off -sym on -sym codeview4 -sym internal -DDEBUGGING -DDKFBPON
BLDMESG		= $(BLDMESG) Using /d1 option
!ifdef NLM_NAME8
LDFLAGS		= $(LDFLAGS) -sym on -sym codeview4 -sym internal -osym $(MAKE_TYPE)\$(NLM_NAME8).sym
!else												# !ifdef NLM_NAME8
LDFLAGS		= $(LDFLAGS) -sym on -sym codeview4 -sym internal -osym $(MAKE_TYPE)\$(NLM_NAME).sym
!endif												# !ifdef NLM_NAME8
!endif												# !ifdef USE_D2
!else												# !if "$(MAKE_TYPE)"=="Debug"
BLDDIR      = $(REL_DIR)
BLDMESG		= Release version
##BS_CFLAGS	= -opt speed -inline smart -inline auto -sym off
BS_CFLAGS	= 
!endif									 			# !if "$(MAKE_TYPE)"=="Debug"


ADD_LOCDEFS =	-DPERL_CORE

NLM_INCLUDE			= -I$(NLMSDKBASE)\include
NLM_INCLUDE_NLM		= -I$(NLMSDKBASE)\include\nlm
NLM_INCLUDE_NLM_SYS = -I$(NLMSDKBASE)\include\nlm\sys
NLM_INCLUDE_OBSLETE = -I$(NLMSDKBASE)\include\nlm\obsolete

!ifdef SECURITYBASE
SECURITY_INCLUDE	= -I$(SECURITYBASE)\include
!endif											#!ifdef SECURITYBASE

!ifdef UCSINC
NLM_INCLUDE_UCS     = -I$(UCSINC)
!endif											#!ifndef UCSINC

!if "$(NW_EXTNS)"=="yes" 
INCLUDE_NW          = -I.\include
!endif


INC_PREV			= -I..
INC_THIS			= -I.


NLM_INCLUDE_PATH = $(NLM_INCLUDE) $(NLM_INCLUDE_NLM) $(NLM_INCLUDE_NLM_SYS) $(NLM_INCLUDE_OBSLETE) \
                   $(NLM_INCLUDE_MP) $(TOOL_HEADERS)

!ifdef SECURITYBASE
NLM_INCLUDE_PATH = $(NLM_INCLUDE_PATH) $(SECURITY_INCLUDE)
!endif											#!ifdef SECURITYBASE

!ifdef UCSINC
NLM_INCLUDE_PATH = $(NLM_INCLUDE_PATH) $(NLM_INCLUDE_UCS)
!endif											#!ifndef UCSINC

INCLUDE = $(INC_THIS) $(INC_PREV) -I- $(NLM_INCLUDE_PATH)

PATH = $(PATH);$(TOOL_PATH)
 
NLM_INCLUDES	= -I$(COREDIR) $(INCLUDE_NW)

CCFLAGS     = $(CCFLAGS) $(INCLUDE)

COMPLER_FLAGS	=  $(BS_CFLAGS) $(ADD_BUILDOPT) $(NW_FLAGS) $(COMPILER_FLAG) $(MPKFLAGS) $(CCFLAGS)
                  

# Source file list
NW_H_FILES			=	\
						.\iperlhost.h	\
						.\interface.h	\
						.\netware.h \
						.\nw5iop.h	\
						.\nw5sck.h	\
						.\nwpipe.h	\
						.\nwplglob.h	\
						.\nwtinfo.h	\
						.\nwutil.h	\
						.\nwhashcls.h \

NW_HOST_H_FILES		=	\
						.\iperlhost.h	\
						.\interface.h	\
						.\netware.h	\
						.\nw5sck.h	\
						.\nwperlhost.h	\


CLIB_H_FILES		=	\
						.\clibsdio.h	\
						.\clibstr.h		\
						.\clibstuf.h	\
						.\stdio.h	\
						.\string.h	\

NW_SRC				=	\
						.\CLIBstuf.c	\
						.\sv_nw.c		\
						.\nw5.c		\
						.\nw5sck.c	\
						.\nw5thread.c \
						.\nwmain.c	\
						.\nwpipe.c  \
						.\nwplglob.c	\
						.\nwtinfo.c \
						.\nwutil.c  \

NW_CPP_SRC          =   \
						.\nwhashcls.cpp \
						.\interface.cpp \
						.\perllib.cpp   \

EXT_MAIN_SRC		=	\
						.\Main.c	\

PERL_IO_SRC			=	\
						..\perlio.c	

CLIBSTUF_OBJ		=	\
						.\CLIBstuf.obj

#PERL_TMP_OBJ       = $(PERL_TEMP_SRC:.c=.obj)
NW_SRC_OBJ			= $(NW_SRC:.c=.obj)
NW_CPP_SRC_OBJ      = $(NW_CPP_SRC:.cpp=.obj)
NLM_MICROCORE_OBJ	= $(MICROCORE_SRC:.c=.obj) 
PERL_LIB_OBJ		= $(PERL_LIB_SRC:.c=.obj)
PERL_IO_OBJ			= $(PERL_IO_SRC:.c=.obj)
NLM_CORE_OBJ	    = $(NLM_MICROCORE_OBJ)
EXT_MAIN_OBJ		= $(EXT_MAIN_SRC:.c=.obj)

# For dependency checking 
# $(BLDDIR) in place of Release or Debug is not working, should look into this - sgp
!if "$(BLDDIR)"=="Release" 
NLM_OBJ				= $(NLM_CORE_OBJ:..\=.\Release\)
NEWTARE_OBJ_DEP		= $(NW_SRC_OBJ:.\=.\Release\)
NEWTARE_CPP_OBJ_DEP	= $(NW_CPP_SRC_OBJ:.\=.\Release\)
PERL_LIB_OBJ_DEP	= $(PERL_LIB_OBJ:.\=.\Release\)
PERL_IO_OBJ_DEP		= $(PERL_IO_OBJ:..\=.\Release\)
!else
NLM_OBJ				= $(NLM_CORE_OBJ:..\=.\Debug\)
NEWTARE_OBJ_DEP		= $(NW_SRC_OBJ:.\=.\Debug\)
NEWTARE_CPP_OBJ_DEP	= $(NW_CPP_SRC_OBJ:.\=.\Debug\)
PERL_LIB_OBJ_DEP	= $(PERL_LIB_OBJ:.\=.\Debug\)
PERL_IO_OBJ_DEP		= $(PERL_IO_OBJ:..\=.\Debug\)
!endif

# Symbol base_import & version added for NETWARE
NW_CFG_VARS = \
		"INST_DRV=$(INST_DRV)"			\
		"INST_TOP=$(INST_TOP)"			\
		"INST_VER=$(INST_VER)"			\
		"INST_ARCH=$(INST_ARCH)"		\
		"INST_NW_TOP1=$(INST_NW_TOP1)"	\
		"INST_NW_TOP2=$(INST_NW_TOP2)"	\
		"INST_NW_VER=$(INST_NW_VER)"	\
		"archname=$(ARCHNAME)"			\
		"cc=$(C_COMPILER)"				\
		"ar=$(LINK)"				\
		"ccflags=$(COMPLER_FLAGS)"	\
		"cf_email=$(EMAIL)"			\
		"d_crypt=$(D_CRYPT)"			\
		"d_mymalloc=$(PERL_MALLOC)"		\
#		"libs=$(LIBFILES)"			\
		"incpath=$(NLM_INCLUDE_PATH)"	\
		"libperl=$(PERLIMPLIB:..\=)"		\
		"libpth=$(LIBPATH)"	\
#		"libc=$(LIBC)"				\
		"make=nmake"				\
		"static_ext=$(STATIC_EXT)"		\
		"dynamic_ext=$(DYNAMIC_EXT)"		\
		"nonxs_ext=$(NONXS_EXT)"		\
		"use5005threads=$(USE_5005THREADS)"	\
		"useithreads=$(USE_ITHREADS)"		\
		"usethreads=$(USE_5005THREADS)"		\
		"usemultiplicity=$(USE_MULTI)"		\
		"ld=$(LINK)"			\
		"base_import=$(BASE_IMPORT_FILES)"	\
		"LINK_FLAGS=$(LINK_FLAGS:"=\")"		\
		"optimize="		\
		"d_stdio_cnt_lval=undef"		\
		"d_stdio_ptr_lval=undef"		\
		"d_stdiobase=undef"				\
		"d_stdstdio=undef"				\
		"d_times=undef"					\
		"direntrytype=DIR"				\
		"nlm_version=$(NLM_VERSION)"		\
		"d_archname=NetWare"	\
		"mpktool=$(MPKTOOL) $(XDCFLAGS)"	\
		"toolpath=$(TOOL_PATH)"


NW_CFGSH_TMPL	= config.wc
NW_CFGH_TMPL	= config_H.wc

SOCKET_NLM	= $(AUTODIR)\Socket\Socket.NLM
FCNTL_NLM	= $(AUTODIR)\Fcntl\Fcntl.NLM
IO_NLM		= $(AUTODIR)\IO\IO.NLM
OPCODE_NLM	= $(AUTODIR)\Opcode\Opcode.NLM
SDBM_FILE_NLM = $(AUTODIR)\SDBM_File\SDBM_File.NLM
POSIX_NLM	= $(AUTODIR)\POSIX\POSIX.NLM
ATTRS_NLM	= $(AUTODIR)\attrs\attrs.NLM
THREAD_NLM	= $(AUTODIR)\Thread\Thread.NLM
B_NLM		= $(AUTODIR)\B\B.NLM
DUMPER_NLM	= $(AUTODIR)\Data\Dumper\Dumper.NLM
PEEK_NLM	= $(AUTODIR)\Devel\Peek\Peek.NLM
RE_NLM		= $(AUTODIR)\re\re.NLM
BYTELOADER_NLM	= $(AUTODIR)\ByteLoader\ByteLoader.NLM
DPROF_NLM	= $(AUTODIR)\Devel\DProf\DProf.NLM
GLOB_NLM	= $(AUTODIR)\File\Glob\Glob.NLM
HOSTNAME_NLM	= $(AUTODIR)\Sys\Hostname\Hostname.NLM
CWD_NLM			= $(EXTDIR)\Cwd\Cwd.NLM
STORABLE_NLM		= $(EXTDIR)\Storable\Storable.NLM
LISTUTIL_NLM		= $(EXTDIR)\List\Util.NLM
MIMEBASE64_NLM		= $(EXTDIR)\MIME\Base64\Base64.NLM
XSAPITEST_NLM		= $(EXTDIR)\XS\APItest\APItest.NLM
XSTYPEMAP_NLM		= $(EXTDIR)\XS\Typemap\Typemap.NLM
UNICODENORMALIZE_NLM 	= $(EXTDIR)\Unicode\Normalize\Normalize.NLM

EXTENSION_NLM	= 		\
		$(FCNTL_NLM)	\
		$(BYTELOADER_NLM)	\
		$(IO_NLM)		\
		$(SOCKET_NLM)	\
		$(OPCODE_NLM)	\
		$(B_NLM)		\
		$(ATTRS_NLM)	\
		$(SDBM_FILE_NLM)	\
		$(POSIX_NLM)	\
		$(THREAD_NLM)	\
		$(DUMPER_NLM)	\
		$(GLOB_NLM)		\
		$(PEEK_NLM)		\
		$(RE_NLM)	\
		$(DPROF_NLM)	\
		$(HOSTNAME_NLM)		\
		$(CWD_NLM)		\
		$(STORABLE_NLM)		\
		$(LISTUTIL_NLM)		\
		$(MIMEBASE64_NLM)	\
		$(XSAPITEST_NLM)	\
		$(XSTYPEMAP_NLM)	\
		$(UNICODENORMALIZE_NLM)  \
		$(FILTER_NLM)	

# Begin - Following is required to build NetWare specific extensions CGI2Perl, Perl2UCS and UCSExt
CGI2PERL		= CGI2Perl\CGI2Perl
PERL2UCS		= Perl2UCS\Perl2UCS
UCSExt			= Perl2UCS\UCSExt

CGI2PERL_NLM = \CGI2Perl\CGI2Perl.NLM
PERL2UCS_NLM = $(AUTODIR)\Perl2UCS\Perl2UCS.NLM
UCSExt_NLM = $(AUTODIR)\UCSExt\UCSExt.NLM

NETWARE_EXTNS =	\
		$(CGI2PERL_NLM) \
		$(PERL2UCS_NLM)	\
		$(UCSExt_NLM)
# End


ECHO_SRC			= TestNLM\echo\echo.c
TYPE_SRC			= TestNLM\type\type.c
ECHO_SRC_OBJ			= $(ECHO_SRC:.c=.obj)
TYPE_SRC_OBJ			= $(TYPE_SRC:.c=.obj)
ECHO_NLM	= TestNLM\echo\echo.nlm
TYPE_NLM	= TestNLM\type\type.nlm

TEST_NLMS	=	\
			$(ECHO_NLM)	\
			$(TYPE_NLM)	\


!ifndef SCREEN
SCREEN          = 'none'
!endif

!ifndef NLM_DESCRIPTION
NLM_DESCRIPTION = $(NLM_NAME8) for NetWare
!endif

!ifndef NLM_VERSION
NLM_VERSION    = 3.1.0
!endif

!ifndef NLM_EXT
NLM_EXT         = NLM
!endif

!ifndef BUILT
BUILT     = $(BLDDIR)\$(NLM_NAME8).$(NLM_EXT)
!endif

!ifndef BASE_IMPORT_FILES
BASE_IMPORT_FILES = Import @$(NLMIMPORTS)\clib.imp, @$(NLMIMPORTS)\nlmlib.imp, @$(NLMIMPORTS)\threads.imp, @$(NLMIMPORTS)\nit.imp, @$(NLMIMPORTS)\socklib.imp, \
@$(NLMIMPORTS)\fpsm.imp, @$(NLMIMPORTS)\lib0.imp
!endif

!ifdef SECURITYBASE
BASE_IMPORT_FILES = $(BASE_IMPORT_FILES), @$(SECURITY_IMPORTS)\nwsec.imp
!endif											# !ifdef SECURITYBASE

!ifdef USE_MPK
BASE_IMPORT_FILES = $(BASE_IMPORT_FILES), @$(MPKBASE)\import\mpkorg.imp
!endif

!ifndef BASE_IMPORT_FNS
BASE_IMPORT_FNS = Import ImportSymbol, GetSystemConsoleScreen, LoadModule
!endif

!ifdef CODEWAR
NWLIBPATH  = 
LIBPATH386 = 
LIBPATH = 
!else											# !ifdef CODEWAR
!error Please define the tools base directory before proceeding
!endif											# !ifdef CODEWAR

!ifndef BASE_LIBRARIES
!ifdef WATCOM
BASE_LIBRARIES = Library plib3s.lib, math3s.lib, clib3s.lib
!endif											# !ifdef WATCOM
!ifdef CODEWAR
BASE_LIBRARIES = 
!endif											# !ifdef CODEWAR
!endif											# !ifndef BASE_LIBRARIES


COPYRIGHT = Copyright (C) 2000-01, 2002 Novell, Inc. All Rights Reserved.

EXPORTS = Export @perl.imp

#
# Set these to wherever you want "nmake install" to put your
# newly built perl.
#
INST_DRV	= C:
INST_TOP	= $(INST_DRV)\perl

INST_NW_DRV = i:
INST_NW_VOL = sys:
INST_NW_TOP1 = $(INST_NW_VOL)\perl
INST_NW_TOP2 = $(INST_NW_DRV)\perl
#INST_NW_VER = \5.6.1

#
# Comment this out if you DON'T want your perl installation to be versioned.
# This means that the new installation will overwrite any files from the
# old installation at the same INST_TOP location.  Leaving it enabled is
# the safest route, as perl adds the extra version directory to all the
# locations it installs files to.  If you disable it, an alternative
# versioned installation can be obtained by setting INST_TOP above to a
# path that includes an arbitrary version string.
#
INST_VER	= \5.8.3

#
# Comment this out if you DON'T want your perl installation to have
# architecture specific components.  This means that architecture-
# specific files will be installed along with the architecture-neutral
# files.  Leaving it enabled is safer and more flexible, in case you
# want to build multiple flavors of perl and install them together in
# the same location.  Commenting it out gives you a simpler
# installation that is easier to understand for beginners.
#
INST_ARCH	= \$(ARCHNAME)

#
# uncomment to enable multiple interpreters.  This is need for fork()
# emulation.
#

USE_MULTI	= define

#
# Beginnings of interpreter cloning/threads; still very incomplete.
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

# uncomment this to enable the experimental PerlIO I/O subsystem
# else USE_STDIO will be defined.
#USE_PERLIO	= define
#USE_STDIO = define
#
# WARNING! This option is deprecated and will eventually go away (enable
# USE_ITHREADS instead).
#
# uncomment to enable threads-capabilities.  This is incompatible with
# USE_ITHREADS, and is only here for people who may have come to rely
# on the experimental Thread support that was in 5.005.
#
#USE_5005THREADS= define

# For now let this be here
#
#CRYPT_SRC	= fcrypt.c

# For now let this be here
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

!IF "$(PERL_MALLOC)" == ""
PERL_MALLOC	= undef
!ENDIF

!IF "$(USE_5005THREADS)" == ""
USE_5005THREADS	= undef
!ENDIF

!IF "$(USE_5005THREADS)" == "define"
USE_ITHREADS	= undef
!ENDIF

!IF "$(USE_IMP_SYS)" == "define"
PERL_MALLOC	= undef
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

!IF "$(USE_MULTI)$(USE_5005THREADS)" != "undefundef"
BUILDOPT	= $(BUILDOPT) -DPERL_IMPLICIT_CONTEXT
!ENDIF

!IF "$(USE_IMP_SYS)" != "undef"
BUILDOPT	= $(BUILDOPT) -DPERL_IMPLICIT_SYS
!ENDIF

!IF "$(PROCESSOR_ARCHITECTURE)" == ""
PROCESSOR_ARCHITECTURE	= x86
!ENDIF

!IF "$(USE_5005THREADS)" == "define"
ARCHNAME	= NetWare-$(PROCESSOR_ARCHITECTURE)-thread
!ELSE
!IF "$(USE_MULTI)" == "define"
ARCHNAME	= NetWare-$(PROCESSOR_ARCHITECTURE)-multi
!ELSE
ARCHNAME	= NetWare-$(PROCESSOR_ARCHITECTURE)
!ENDIF
!ENDIF

!IF "$(USE_MULTI)$(USE_5005THREADS)" != "undefundef"
ADD_BUILDOPT	= $(ADD_BUILDOPT) -DPERL_IMPLICIT_CONTEXT
!ENDIF

!IF "$(USE_IMP_SYS)" != "undef"
ADD_BUILDOPT	= $(ADD_BUILDOPT) -DPERL_IMPLICIT_SYS
!ENDIF

!IF "$(USE_ITHREADS)" == "define"
ARCHNAME	= $(ARCHNAME)-thread
!ENDIF

!IF "$(USE_PERLIO)" == "define"
USE_STDIO		= undef
ADD_BUILDOPT	= $(ADD_BUILDOPT) -DUSE_PERLIO
ARCHNAME		= $(ARCHNAME)-perlio
!ELSE
#USE_STDIO	= define
#ADD_BUILDOPT	= $(ADD_BUILDOPT) -DUSE_STDIO
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
INST_HTML	= $(INST_POD)\html

#
# Options
#

OBJOUT_FLAG	= -Fo
EXEOUT_FLAG	= -Fe


#################### do not edit below this line #######################
############# NO USER-SERVICEABLE PARTS BEYOND THIS POINT ##############

o = .obj

#
# Rules
# 

.SUFFIXES : .c $(o) .nlm .lib .NLM


#
# various targets
PERLIMPLIB	= ..\perl.lib

MINIPERL	= ..\miniperl.exe
CONFIGPM	= ..\lib\Config.pm
MINIMOD		= ..\lib\ExtUtils\Miniperl.pm
X2P			= ..\x2p\a2p.nlm

PL2BAT		= ..\win32\bin\pl2bat.pl

UTILS		=			\
		..\utils\h2ph		\
		..\utils\splain		\
		..\utils\dprofpp	\
		..\utils\perlbug	\
		..\utils\pl2pm 		\
		..\utils\c2ph		\
		..\utils\h2xs		\
		..\utils\perldoc	\
		..\utils\perlcc		\
		..\pod\checkpods	\
		..\pod\pod2html		\
		..\pod\pod2latex	\
		..\pod\pod2man		\
		..\pod\pod2text		\
		..\pod\pod2usage	\
		..\pod\podchecker	\
		..\pod\podselect	\
		..\x2p\find2perl	\
		..\x2p\s2p		

MAKE		= nmake -nologo
#NMAKE       = $(C_COMPILER) $(INCLUDE) $(NLM_INCLUDES) $(COMPLER_FLAGS) $(ADD_LOCDEFS) $(ERROR_FLAG) $(*F).c -o $@

XCOPY		= xcopy /f /r /i /d
RCOPY		= xcopy /f /r /i /e /d
NOOP		= @echo
NULL		=

#
# filenames given to xsubpp must have forward slashes (since it puts
# full pathnames in #line strings)
XSUBPP		= ..\$(MINIPERL) -I..\..\lib ..\$(EXTUTILSDIR)\xsubpp -C++ -prototypes

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

#EXTRACORE_SRC	= $(EXTRACORE_SRC) perllib.c

!IF "$(PERL_MALLOC)" == "define"
EXTRACORE_SRC	= $(EXTRACORE_SRC) ..\malloc.c
!ENDIF

#EXTRACORE_SRC	= $(EXTRACORE_SRC) ..\perlio.c

!IF "$(CRYPT_SRC)" != ""
NW_SRC	= $(NW_SRC) .\$(CRYPT_SRC)
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
		..\thrdvar.h	
		
CORE_H		= $(CORE_NOCFG_H) .\config.h

DLL_OBJ		= $(DLL_SRC:.c=.obj)
X2P_OBJ		= $(X2P_SRC:.c=.obj)

DYNAMIC_EXT	= Socket IO Fcntl Opcode SDBM_File POSIX attrs Thread B re \
		Data/Dumper Devel/Peek ByteLoader Devel/DProf File/Glob \
		Storable/Storable List/Util MIME/Base64/Base64 XS/APItest/APItest \
		XS/Typemap/Typemap Unicode/Normalize/Normalize Sys/Hostname

STATIC_EXT	= DynaLoader
NONXS_EXT	= Errno

DYNALOADER	= $(EXTDIR)\DynaLoader\DynaLoader
SOCKET		= $(EXTDIR)\Socket\Socket
FCNTL		= $(EXTDIR)\Fcntl\Fcntl
OPCODE		= $(EXTDIR)\Opcode\Opcode
SDBM_FILE	= $(EXTDIR)\SDBM_File\SDBM_File
IO			= $(EXTDIR)\IO\IO
POSIX		= $(EXTDIR)\POSIX\POSIX
ATTRS		= $(EXTDIR)\attrs\attrs
THREAD		= $(EXTDIR)\Thread\Thread
B			= $(EXTDIR)\B\B
RE			= $(EXTDIR)\re\re
DUMPER		= $(EXTDIR)\Data\Dumper\Dumper
ERRNO		= $(EXTDIR)\Errno\Errno
PEEK		= $(EXTDIR)\Devel\Peek\Peek
BYTELOADER	= $(EXTDIR)\ByteLoader\ByteLoader
DPROF		= $(EXTDIR)\Devel\DProf\DProf
GLOB		= $(EXTDIR)\File\Glob\Glob
HOSTNAME	= $(EXTDIR)\Sys\Hostname\Hostname
CWD			= $(EXTDIR)\Cwd\Cwd
STORABLE		= $(EXTDIR)\Storable\Storable
LISTUTIL		= $(EXTDIR)\List\Util
MIMEBASE64		= $(EXTDIR)\MIME\Base64\Base64
XSAPITEST		= $(EXTDIR)\XS\APItest\APItest
XSTYPEMAP		= $(EXTDIR)\XS\Typemap\Typemap
UNICODENORMALIZE	= $(EXTDIR)\Unicode\Normalize\Normalize

ERRNO_PM_NW	= $(LIBDIR)\Errno.pm

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
		$(HOSTNAME).c \
		$(CWD).c	\
		$(STORABLE).c	\
		$(LISTUTIL).c	\
		$(MIMEBASE64).c	\
		$(XSAPITEST).c	\
		$(XSTYPEMAP).c	\
		$(UNICODENORMALIZE).c	\

EXTENSION_NPM =	\
		$(ERRNO_PM_NW)	\

POD2HTML	= $(PODDIR)\pod2html
POD2MAN		= $(PODDIR)\pod2man
POD2LATEX	= $(PODDIR)\pod2latex
POD2TEXT	= $(PODDIR)\pod2text

#
# Top targets
#

all : .cleanoldfiles .\nwconfig.h $(CONFIGPM) $(NLM_NAME) $(EXTENSION_NLM) $(EXTENSION_NPM) $(TEST_NLMS) $(NETWARE_EXTNS)

#------------------------------------------------------------

..\config.sh : config.nw5 $(MINIPERL) config_sh.PL
	$(MINIPERL) -I..\lib config_sh.PL $(NW_CFG_VARS) config.nw5 > ..\config.sh

# this target is for when changes to the main config.sh happen
# edit config.{b,v,g,w}c and make this target once for each supported
# compiler (e.g. `dmake CCTYPE=BORLAND regen_config_h`)
regen_config_h:
	perl config_sh.PL $(NW_CFG_VARS) $(NW_CFGSH_TMPL) > ..\config.sh
	cd ..
	-del /f /q perl.exe
	perl configpm
	cd netware
	-del /f /q $(NW_CFGH_TMPL)
	-mkdir $(COREDIR)
	-perl -I..\lib config_h.PL "INST_VER=$(INST_VER)"
	rename config.h $(NW_CFGH_TMPL)

$(CONFIGPM) : $(MINIPERL) ..\config.sh config_h.PL ..\minimod.pl
	cd .. && miniperl configpm
	if exist lib\* $(RCOPY) lib\*.* ..\lib\$(NULL)
	$(XCOPY) ..\*.h $(COREDIR)\*.*
	$(XCOPY) *.h $(COREDIR)\*.*
	$(XCOPY) ..\ext\re\re.pm $(LIBDIR)\*.*
	if exist include\* $(RCOPY) include $(COREDIR)\*.*
	$(MINIPERL) -I..\lib config_h.PL "INST_VER=$(INST_VER)" \
	    || $(MAKE) /$(MAKEFLAGS) $(CONFIGPM)
        @echo CONFIGPM Done

$(MINIPERL) : 
	$(error)Please build $(MINIPERL) before continuing

$(MINIMOD) : $(MINIPERL) ..\minimod.pl
	cd .. && miniperl minimod.pl > lib\ExtUtils\Miniperl.pm

..\x2p\a2p$(o) : ..\x2p\a2p.c
	@echo $(MPKMESSAGE)...$(BLDMESG)...$@
	$(C_COMPILER) $(COMPLER_FLAGS) $(NLM_INCLUDES) -I..\x2p $(ADD_LOCDEFS) $(ERROR_FLAG) $*.c -o $@
    @echo Built $(@)

..\x2p\hash$(o) : ..\x2p\hash.c
	@echo $(MPKMESSAGE)...$(BLDMESG)...$@
	$(C_COMPILER) $(COMPLER_FLAGS) $(NLM_INCLUDES) -I..\x2p $(ADD_LOCDEFS) $(ERROR_FLAG) $*.c -o $@
    @echo Built $(@)
	
..\x2p\str$(o) : ..\x2p\str.c
	@echo $(MPKMESSAGE)...$(BLDMESG)...$@
	$(C_COMPILER) $(COMPLER_FLAGS) $(NLM_INCLUDES) -I..\x2p $(ADD_LOCDEFS) $(ERROR_FLAG) $*.c -o $@
    @echo Built $(@)

..\x2p\util$(o) : ..\x2p\util.c
	@echo $(MPKMESSAGE)...$(BLDMESG)...$@
	$(C_COMPILER) $(COMPLER_FLAGS) $(NLM_INCLUDES) -I..\x2p $(ADD_LOCDEFS) $(ERROR_FLAG) $*.c -o $@
    @echo Built $(@)

..\x2p\walk$(o) : ..\x2p\walk.c
	@echo $(MPKMESSAGE)...$(BLDMESG)...$@
	$(C_COMPILER) $(COMPLER_FLAGS) $(NLM_INCLUDES) -I..\x2p $(ADD_LOCDEFS) $(ERROR_FLAG) $*.c -o $@
    @echo Built $(@)
	
$(X2P) : $(MINIPERL) $(X2P_OBJ)
	@echo Building $@..........
	$(MINIPERL) ..\x2p\find2perl.PL
	$(MINIPERL) ..\x2p\s2p.PL
# Linker definitions and lining come here for CODEWARRIOR
    @echo $(BASE_IMPORT_FILES) > $*.def
    @echo MODULE clib >> $*.def
    @echo Import @perl.imp >> $*.def
!ifdef USE_XDC
	$(MPKTOOL) $(XDCFLAGS) $*.xdc
	@echo Import Mp.imp >> $*.def
	@echo xdcdata $*.xdc >> $*.def
!endif
##    $(LINK) $(LDFLAGS) $(BS_CFLAGS) -desc "Awk to Perl Translator" $(X2P_OBJ) $(BLDDIR)\clibstuf.obj -commandfile $*.def -o $@ 
    $(LINK) $(LDFLAGS) -desc "Awk to Perl Translator" $(X2P_OBJ) $(BLDDIR)\clibstuf.obj -commandfile $*.def -o $@ 

$(EXTDIR)\DynaLoader\dl_netware.xs: dl_netware.xs
	copy dl_netware.xs $(EXTDIR)\DynaLoader\dl_netware.xs

HEADERS :
	@echo . . . . making stdio.h and string.h
	@copy << stdio.h >\nul

/*
 * Copyright (C) 2000-01 Novell, Inc. All Rights Reserved.
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Artistic License, as specified in the README file.
 *
 */

/*
 * FILENAME		:	stdio.h
 * DESCRIPTION	:	Generated header file, do not edit. See makefile.
 *                  This header file causes the includer to use clibstuf.h
 *                  The purpose of clibstuf is to make sure that Perl, cgi2perl and
 *                  all the perl extension nlm's (*.NLM) use the Novell Netware CLIB versions
 *                  of standard functions. This code loads up a whole bunch of function pointers
 *                  to point at the standard CLIB functions.
 * Author		:	HYAK
 * Date			:	January 2001.
 *
 */


#ifndef __Stdio_H__
#define __Stdio_H__


#include "$(NLMSDKBASE)\INCLUDE\NLM\stdio.h"
#include "clibsdio.h"


#endif	// __Stdio_H__

<<
	@copy stdio.h $(COREDIR)
	 
	@copy << string.h >\nul

/*
 * Copyright (C) 2000-01 Novell, Inc. All Rights Reserved.
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Artistic License, as specified in the README file.
 *
 */

/*
 * FILENAME		:	string.h
 * DESCRIPTION	:	Generated header file, do not edit. See makefile.
 *                  This header file causes the includer to use clibstuf.h
 *                  The purpose of clibstuf is to make sure that Perl, cgi2perl and
 *                  all the perl extension nlm's (*.NLM) use the Novell Netware CLIB versions
 *                  of standard functions. This code loads up a whole bunch of function pointers
 *                  to point at the standard CLIB functions.
 * Author		:	HYAK
 * Date			:	January 2001.
 *
 */


#ifndef __String_H__
#define __String_H__


#include "$(NLMSDKBASE)\INCLUDE\NLM\string.h"
#include "clibstr.h"


#endif	// __String_H__

<<
	@copy string.h $(COREDIR)


$(NLM_NAME): MESSAGE HEADERS $(BLDDIR)\nul $(NLM_OBJ) $(NEWTARE_OBJ_DEP) $(NEWTARE_CPP_OBJ_DEP) $(PERL_IO_OBJ_DEP) $(DLL_OBJ) \
            $(PERLIMPLIB) $(EXT_MAIN_OBJ) $(PERL_TEMP_OBJ) #$(PERL_LIB_OBJ) 
	@echo======= Linking $@ at $(MAKEDIR)\$(BLDDIR) =======
# Linker definitions and lining come here for CODEWARRIOR
	@echo $(BASE_IMPORT_FILES) > $*.def
	@echo MODULE clib >> $*.def
	@echo MODULE netdb >> $*.def
!ifdef SECURITYBASE
   @echo MODULE nwsec >> $*.def
!endif											#!ifdef SECURITYBASE
	@echo $(EXPORTS) >> $*.def
!ifdef USE_XDC
	@echo======= Creating XDC file
	@echo Import Mp.imp >> $*.def
!ifdef NLM_NAME8
	$(MPKTOOL) $(XDCFLAGS) $(BLDDIR)\$(NLM_NAME8).xdc
	@echo xdcdata $(BLDDIR)\$(NLM_NAME8).xdc >> $*.def
!else
	$(MPKTOOL) $(XDCFLAGS) $(BLDDIR)\$(NLM_NAME).xdc
	@echo xdcdata $(BLDDIR)\$(NLM_NAME).xdc >> $*.def 
!endif
!endif
##    $(LINK) $(LDFLAGS) $(BS_CFLAGS) -desc "Perl 5.6.1 for NetWare" $(NEWTARE_OBJ_DEP:.obj=.obj) $(NLM_OBJ:.obj=.obj) $(PERL_IO_OBJ_DEP:.obj=.obj) $(DLL_OBJ:.obj=.obj) $(NEWTARE_CPP_OBJ_DEP:.obj=.obj) -commandfile $*.def -o .\$(BLDDIR)\$@
	$(LINK) $(LDFLAGS) -desc $(MODULE_DESC) $(NEWTARE_OBJ_DEP:.obj=.obj) $(NLM_OBJ:.obj=.obj) $(PERL_IO_OBJ_DEP:.obj=.obj) $(DLL_OBJ:.obj=.obj) $(NEWTARE_CPP_OBJ_DEP:.obj=.obj) -commandfile $*.def -o .\$(BLDDIR)\$@
	copy splittree.pl .. 
	$(MINIPERL) -I..\lib ..\splittree.pl "../LIB" $(AUTODIR)
	@echo ========Linked $@ ==========

	@echo======= Finished building $(BUILT).

# Create the debug or release directory if not existing
$(BLDDIR)\nul:
  	@echo . . . . mkdir $(BLDDIR)
  	@mkdir $(BLDDIR)
	@echo '$(BLDDIR)' directory created.

MESSAGE: 
	@echo======= $(MAKE_ACTION)ing $(NLM_NAME) at $(MAKEDIR)\$(BLDDIR) ======= 
  
$(PERLIMPLIB): perllib.imp
#    @echo Building $(PERLIMPLIB)...
#    $(LD) -type library $(NLM_OBJ) $(BLDDIR)\nw5.obj $(BLDDIR)\nwmain.obj $(BLDDIR)\nw5thread.obj $(BLDDIR)\nwtinfo.obj \
#    $(BLDDIR)\nwutil.obj $(BLDDIR)\interface.obj $(BLDDIR)\perllib.obj $(PERL_IO_OBJ_DEP) $(DLL_OBJ) -o $@
#	$(XCOPY) $(PERLIMPLIB) $(COREDIR)
#	@echo $(PERLIMPLIB) Done

perllib.imp : $(MINIPERL) $(CONFIGPM) ..\global.sym ..\pp.sym ..\makedef.pl
#	$(MINIPERL) -w ..\makedef.pl PLATFORM=netware FILETYPE=def $(ADD_BUILDOPT) \
#	    CCTYPE=$(CCTYPE) > perllib.def
	@echo (Perl) > perl.imp
  	$(MINIPERL) -w ..\makedef.pl PLATFORM=netware FILETYPE=imp $(BS_CFLAGS) $(DEFINES) $(ADD_BUILDOPT) \
	    CCTYPE=$(CCTYPE) >> perl.imp
	copy perl.imp $(COREDIR)
  
$(DLL_OBJ) : $(DYNALOADER).c $(CORE_H) $(EXTDIR)\DynaLoader\dlutils.c
  	@echo $(MPKMESSAGE)...$(BLDMESG)...$@
	@$(C_COMPILER) $(NLM_INCLUDES) $(COMPLER_FLAGS) $(NLM_INCLUDES) $(ADD_LOCDEFS) $(ERROR_FLAG) -I$(EXTDIR)\DynaLoader \
	$(EXTDIR)\DynaLoader\$(*F).c -o $@
	@echo $(@) Done.

$(DYNALOADER).c : $(MINIPERL) $(EXTDIR)\DynaLoader\dl_netware.xs $(CONFIGPM)
	if not exist $(AUTODIR) mkdir $(AUTODIR)
	cd $(EXTDIR)\$(*B)
	..\$(MINIPERL) -I..\..\lib $(*B)_pm.PL
	..\$(MINIPERL) -I..\..\lib XSLoader_pm.PL
	cd ..\..\netware
	$(XCOPY) $(EXTDIR)\$(*B)\$(*B).pm $(LIBDIR)\$(NULL)
	$(XCOPY) $(EXTDIR)\$(*B)\XSLoader.pm $(LIBDIR)\$(NULL)
	cd $(EXTDIR)\$(*B)
	$(XSUBPP) dl_netware.xs > $(*B).c
	cd ..\..\netware
	@echo Dynaloader Done


$(PERL_IO_OBJ_DEP) : ..\$(*F).c
	@echo $(MPKMESSAGE) $(BLDMESG) $@
	$(C_COMPILER) $(COMPLER_FLAGS) $(NLM_INCLUDES) $(ADD_LOCDEFS) $(ERROR_FLAG) ..\$(*F).c -o $@
	@echo Built $(@)

$(NLM_OBJ)	: ..\$(*F).c
	@echo $(MPKMESSAGE) $(BLDMESG) $@
	$(C_COMPILER) $(COMPLER_FLAGS) $(NLM_INCLUDES) $(ADD_LOCDEFS) $(ERROR_FLAG) ..\$(*F).c -o $@
	@echo Built $(@)
	
    
$(NEWTARE_OBJ_DEP) : $(NW_H_FILES) $(NW_HOST_H_FILES) $(*F).c
	@echo $(MPKMESSAGE) $(BLDMESG) $@
	$(C_COMPILER) $(COMPLER_FLAGS) $(NLM_INCLUDES) $(ADD_LOCDEFS) $(ERROR_FLAG) $(*F).c -o $@
    @echo Built $(@)

$(NEWTARE_CPP_OBJ_DEP) : $(NW_H_FILES) $(NW_HOST_H_FILES) $(*F).cpp
	@echo $(MPKMESSAGE) $(BLDMESG) $@
	$(C_COMPILER) $(CWCPPFLAGS) $(COMPLER_FLAGS) $(NLM_INCLUDES) $(ADD_LOCDEFS) $(ERROR_FLAG) $(*F).cpp -o $@
    @echo Built $(@)

$(EXT_MAIN_OBJ) : $(CLIB_H_FILES)
    @echo $(MPKMESSAGE) $(BLDMESG) $@
    $(C_COMPILER)  $(NLM_INCLUDES) $(COMPLER_FLAGS) $(ADD_LOCDEFS) $(ERROR_FLAG) $(*F).c -o $@	
    $(LD) -type library $@ -o $*.lib
    @copy $*.lib $(COREDIR)

# Delete any files that might have got created during building miniperl.exe
# config.sh will definitely be created
# COREDIR might have got created
.cleanoldfiles :
	-del /f /q $(PERLIMPLIB)
	-del /f /q ..\lib\config.pm
	-del /f /q ..\config.sh
	-del /f /q .\Main.obj
	-del /f /q .\Main.lib
	-rmdir /s /q $(AUTODIR)
	-rmdir /s /q $(COREDIR)
	-del /f /q ..\lib\core

.\nwconfig.h : $(NW_CFGH_TMPL)
	@if exist .\config.h del /f /q .\config.h
	copy $(NW_CFGH_TMPL) config.h

# REQUIRED WHEN WE INCLUDE CONFIGPM OR REGEN_CONFIG - sgp
#..\nwconfig.sh : config.nw5 $(MINIPERL) config_sh.PL
#	$(MINIPERL) -I..\lib config_sh.PL $(NW_CFG_VARS) config.nw5 > ..\config.sh
#	@pause
#	cd ..
#	del /f /q config.sh
#	rename nwconfig.sh config.sh
#	cd netware

config.nw5 : $(NW_CFGSH_TMPL)
	copy $(NW_CFGSH_TMPL) config.nw5

$(SOCKET_NLM): $(NLM_NAME) $(SOCKET).xs
	cd $(EXTDIR)\$(*B)
	..\..\miniperl -I..\..\lib Makefile.PL PERL_CORE=1 INSTALLDIRS=perl
	$(MAKE)
	cd ..\..\netware

$(HOSTNAME_NLM): $(NLM_NAME) $(HOSTNAME).xs
	cd $(EXTDIR)\Sys\$(*B)
	..\..\..\miniperl -I..\..\..\lib Makefile.PL PERL_CORE=1 INSTALLDIRS=perl
	$(MAKE)
	cd ..\..\..\netware

$(FCNTL_NLM):
	cd $(EXTDIR)\$(*B)
	..\..\miniperl -I..\..\lib Makefile.PL PERL_CORE=1 INSTALLDIRS=perl
	$(MAKE)
	cd ..\..\netware

$(IO_NLM):
	cd $(EXTDIR)\$(*B)
	..\..\miniperl -I..\..\lib Makefile.PL PERL_CORE=1 INSTALLDIRS=perl
	$(MAKE)
	cd ..\..\netware

$(OPCODE_NLM):
	cd $(EXTDIR)\$(*B)
	..\..\miniperl -I..\..\lib Makefile.PL PERL_CORE=1 INSTALLDIRS=perl
	$(MAKE)
	cd ..\..\netware

$(B_NLM):
	cd $(EXTDIR)\$(*B)
	..\..\miniperl -I..\..\lib Makefile.PL PERL_CORE=1 INSTALLDIRS=perl
	$(MAKE)
	cd ..\..\netware

$(DUMPER_NLM):
	cd $(EXTDIR)\Data\$(*B)
	..\..\..\miniperl -I..\..\..\lib Makefile.PL PERL_CORE=1 INSTALLDIRS=perl
	$(MAKE)
	cd ..\..\..\netware

$(PEEK_NLM):
	cd $(EXTDIR)\Devel\$(*B)
	..\..\..\miniperl -I..\..\..\lib Makefile.PL PERL_CORE=1 INSTALLDIRS=perl
	$(MAKE)
	cd ..\..\..\netware

$(RE_NLM):
	cd $(EXTDIR)\$(*B)
	..\..\miniperl -I..\..\lib Makefile.PL PERL_CORE=1 INSTALLDIRS=perl
	$(MAKE)
	cd ..\..\netware

$(BYTELOADER_NLM):
	cd $(EXTDIR)\$(*B)
	..\..\miniperl -I..\..\lib Makefile.PL PERL_CORE=1 INSTALLDIRS=perl
	$(MAKE)
	cd ..\..\netware

$(DPROF_NLM):
	cd $(EXTDIR)\Devel\$(*B)
	..\..\..\miniperl -I..\..\..\lib Makefile.PL PERL_CORE=1 INSTALLDIRS=perl
	$(MAKE)
	cd ..\..\..\netware

$(GLOB_NLM):
	cd $(EXTDIR)\File\$(*B)
	..\..\..\miniperl -I..\..\..\lib Makefile.PL PERL_CORE=1 INSTALLDIRS=perl
	$(MAKE)
	cd ..\..\..\netware

$(POSIX_NLM):
	cd $(EXTDIR)\$(*B)
	..\..\miniperl -I..\..\lib Makefile.PL PERL_CORE=1 INSTALLDIRS=perl
	$(MAKE)
	cd ..\..\netware

$(THREAD_NLM):
	cd $(EXTDIR)\$(*B)
	..\..\miniperl -I..\..\lib Makefile.PL PERL_CORE=1 INSTALLDIRS=perl
	$(MAKE)
	cd ..\..\netware

$(ATTRS_NLM):
	cd $(EXTDIR)\$(*B)
	..\..\miniperl -I..\..\lib Makefile.PL PERL_CORE=1 INSTALLDIRS=perl
	$(MAKE)
	cd ..\..\netware

$(SDBM_FILE_NLM):
	cd $(EXTDIR)\$(*B)
	..\..\miniperl -I..\..\lib Makefile.PL PERL_CORE=1 INSTALLDIRS=perl
	$(MAKE)
	cd ..\..\netware

$(CWD_NLM):
	cd $(EXTDIR)\$(*B)
	..\..\miniperl -I..\..\lib Makefile.PL PERL_CORE=1 INSTALLDIRS=perl
	$(MAKE)
	cd ..\..\netware

$(STORABLE_NLM):
	cd $(EXTDIR)\$(*B)
	..\..\miniperl -I..\..\lib Makefile.PL PERL_CORE=1 INSTALLDIRS=perl
	$(MAKE)
	cd ..\..\netware

$(LISTUTIL_NLM):
	cd $(EXTDIR)\List\$(*B)
	..\..\..\miniperl -I..\..\lib Makefile.PL PERL_CORE=1 INSTALLDIRS=perl
	$(MAKE)
	cd ..\..\..\netware

$(MIMEBASE64_NLM):
	cd $(EXTDIR)\Mime\$(*B)
	..\..\..\miniperl -I..\..\lib Makefile.PL PERL_CORE=1 INSTALLDIRS=perl
	$(MAKE)
	cd ..\..\..\netware

$(XSAPITEST_NLM):
	cd $(EXTDIR)\XS\$(*B)
	..\..\..\miniperl -I..\..\lib Makefile.PL PERL_CORE=1 INSTALLDIRS=perl
	$(MAKE)
	cd ..\..\..\netware

$(XSTYPEMAP_NLM):
	cd $(EXTDIR)\XS\$(*B)
	..\..\..\miniperl -I..\..\lib Makefile.PL PERL_CORE=1 INSTALLDIRS=perl
	$(MAKE)
	cd ..\..\..\netware

$(UNICODENORMALIZE_NLM):
	cd $(EXTDIR)\Unicode\$(*B)
	..\..\..\miniperl -I..\..\lib Makefile.PL PERL_CORE=1 INSTALLDIRS=perl
	$(MAKE)
	cd ..\..\..\netware


$(ERRNO_PM_NW):
#	@echo Building $@
	cd $(EXTDIR)\$(*B)
	..\..\miniperl -I..\..\lib Makefile.PL PERL_CORE=1 INSTALLDIRS=perl
	$(MAKE)
	cd ..\..\netware

$(ECHO_SRC_OBJ): $*.c
	@echo $(MPKMESSAGE) $(BLDMESG) $@
	$(C_COMPILER) $(NLM_INCLUDES) $(COMPLER_FLAGS) $(ADD_LOCDEFS) $(ERROR_FLAG) $*.c -o $@
	@echo Built $(@)

$(ECHO_NLM): $(ECHO_SRC_OBJ)
	@echo======= Linking $@ =======
# Linker definitions and lining come here for CODEWARRIOR
    @echo $(BASE_IMPORT_FILES) > $*.def
    @echo MODULE clib >> $*.def
    @echo Import @perl.imp >> $*.def
!ifdef USE_XDC
	$(MPKTOOL) $(XDCFLAGS) $*.xdc
	@echo Import @MP.imp >> $*.def
	@echo xdcdata $*.xdc >> $*.def 
!endif
##    $(LINK) $(LDFLAGS) $(BS_CFLAGS) -desc "DOS Echo emulation for Perl testing" $(ECHO_SRC_OBJ) $(BLDDIR)\clibstuf.obj -commandfile $*.def -o $@
    $(LINK) $(LDFLAGS) -desc "DOS Echo emulation for Perl testing" $(ECHO_SRC_OBJ) $(BLDDIR)\clibstuf.obj -commandfile $*.def -o $@
	@echo======= Linking Complete =======

$(TYPE_SRC_OBJ): $*.c
	@echo $(MPKMESSAGE) $(BLDMESG) $@
	$(C_COMPILER) $(NLM_INCLUDES) $(COMPLER_FLAGS) $(ADD_LOCDEFS) $(ERROR_FLAG) $*.c -o $@
	@echo Built $(@)
	
$(TYPE_NLM): $(TYPE_SRC_OBJ)
	@echo======= Linking $@ =======
# Linker definitions and lining come here for CODEWARRIOR
    @echo $(BASE_IMPORT_FILES) > $*.def
    @echo MODULE clib >> $*.def
    @echo Import @perl.imp >> $*.def
!ifdef USE_XDC
	$(MPKTOOL) $(XDCFLAGS) $*.xdc
	@echo Import @MP.imp >> $*.def 
	@echo xdcdata $*.xdc >> $*.def 
!endif
##    $(LINK) $(LDFLAGS) $(BS_CFLAGS) -desc "DOS Type emulation for Perl testing" $(TYPE_SRC_OBJ) $(BLDDIR)\clibstuf.obj -commandfile $*.def -o $@
    $(LINK) $(LDFLAGS) -desc "DOS Type emulation for Perl testing" $(TYPE_SRC_OBJ) $(BLDDIR)\clibstuf.obj -commandfile $*.def -o $@
	@echo======= Linking Complete =======


# Build NetWare specific extensions
$(CGI2PERL_NLM):
!if "$(NW_EXTNS)"=="yes"
	cd $(*B)
	..\..\miniperl -I..\..\lib Makefile.PL "CCCDLFLAGS=-bool on -lang c++" PERL_CORE=1 INSTALLDIRS=perl
	$(MAKE)
	cd ..\..\netware
!endif

$(PERL2UCS_NLM):
!if "$(NW_EXTNS)"=="yes"
	cd $(*B)
	..\..\miniperl -I..\..\lib Makefile.PL "CCCDLFLAGS=-bool on -lang c++" PERL_CORE=1 INSTALLDIRS=perl
	$(MAKE)
	cd ..\..\netware
!endif

$(UCSExt_NLM):
!if "$(NW_EXTNS)"=="yes"
	cd $(*B)
	..\..\miniperl -I..\..\lib Makefile.PL "CCCDLFLAGS=-bool on -lang c++" PERL_CORE=1 INSTALLDIRS=perl
	$(MAKE)
	cd ..\..\netware
!endif

nwclean:
	-rmdir /s /q $(REL_DIR)
	-rmdir /s /q $(DEB_DIR)
	@if exist .\stdio.h del /f /q .\stdio.h
	@if exist .\string.h del /f /q .\string.h
	@if exist .\config.h del /f /q .\config.h
	@if exist .\config.nw5 del /f /q .\config.nw5
	@if exist .\perl.imp del /f /q .\perl.imp
	-del /f /q *.obj *.lib *.def *.sym *.map *.xdc *.err *.nlm
	cd testnlm\echo
	-del /f /q *.obj *.map *.link *.options *.nlm *.sym *.xdc *.err *.lib *.def *.pdb *.bs
	cd ..\type
	-del /f /q *.obj *.map *.link *.options *.nlm *.sym *.xdc *.err *.lib *.def *.pdb *.bs
	cd ..\..\

utils: $(BLDDIR)\$(NLM_NAME8).$(NLM_EXT) $(X2P)
	cd ..\utils
	$(MAKE) PERL=$(MINIPERL)
	cd ..\pod
	copy ..\README.amiga .\perlamiga.pod
	copy ..\README.cygwin .\perlcygwin.pod
	copy ..\README.dos .\perldos.pod
	copy ..\README.hpux .\perlhpux.pod
#	copy ..\README.machten .\perlmachten.pod
	copy ..\README.os2 .\perlos2.pod
	copy ..\vms\perlvms.pod .\perlvms.pod
	copy ..\README.win32 .\perlwin32.pod
	copy ..\README.netware .\perlnw5.pod
	$(MAKE) -f ..\win32\pod.mak converters

	cd ..\lib
	$(MINIPERL) lib_pm.PL
	cd ..\netware
	$(MINIPERL) $(PL2BAT) $(UTILS)

distclean: clean nwclean
	-del /f /q $(PERLIMPLIB) ..\miniperl.lib $(MINIMOD)
	-del /f /q $(EXTENSION_NPM)
	-del /f /q $(EXTENSION_C) $(DYNALOADER).c $(ERRNO).pm
	-del /f /q $(EXTDIR)\DynaLoader\dl_netware.xs
	-del /f /q $(EXTDIR)\DynaLoader\dl_win32.xs
	-del /f /q $(EXTDIR)\DynaLoader\DynaLoader.pm
	-del /f /q $(EXTDIR)\DynaLoader\XSLoader.pm
	-del /f /q $(LIBDIR)\.exists $(LIBDIR)\attrs.pm $(LIBDIR)\DynaLoader.pm
	-del /f /q $(LIBDIR)\XSLoader.pm
	-del /f /q $(LIBDIR)\Fcntl.pm $(LIBDIR)\IO.pm $(LIBDIR)\Opcode.pm
	-del /f /q $(LIBDIR)\ops.pm $(LIBDIR)\Safe.pm
	-del /f /q $(LIBDIR)\SDBM_File.pm $(LIBDIR)\Socket.pm $(LIBDIR)\POSIX.pm
	-del /f /q $(LIBDIR)\B.pm $(LIBDIR)\O.pm $(LIBDIR)\re.pm
	-del /f /q $(LIBDIR)\Data\Dumper.pm $(LIBDIR)\ByteLoader.pm
	-del /f /q $(LIBDIR)\Devel\Peek.pm $(LIBDIR)\Devel\DProf.pm
	-del /f /q $(LIBDIR)\File\Glob.pm
	-del /f /q $(LIBDIR)\Unicode\Normalize.pm
	-rmdir /s /q $(LIBDIR)\IO
	-rmdir /s /q $(LIBDIR)\Thread
	-rmdir /s /q $(LIBDIR)\B
	-rmdir /s /q $(LIBDIR)\Data
	-del /f /q $(PODDIR)\*.html
	-del /f /q $(PODDIR)\*.bat
	cd ..\utils
	-del /f /q h2ph splain perlbug pl2pm c2ph h2xs perldoc dprofpp
	-del /f /q *.bat
	cd ..\netware
	cd ..\x2p
	-del /f /q find2perl s2p
	-del /f /q *.bat *.exe
	-del /f /q *.obj *.map *.link *.xdc *.err
	cd ..\netware
	-del /f /q ..\config.sh ..\splittree.pl dlutils.c config.h.new
	-del /f /q $(CONFIGPM)
	-del /f /q bin\*.bat
	cd $(EXTDIR)
    -del /s /q /f *.lib *.def *.map *.pdb *.bs Makefile *$(o) pm_to_blib *.xdc *.err *.obj *.sym
	cd ..\netware
!if "$(NW_EXTNS)"=="yes"
	cd cgi2perl
	-del /f /q *.obj *.bs Makefile *$(o) *.c pm_to_blib *.xdc *.err *.sym *.map *.def *.lib *.pdb
	cd ..
	cd Perl2UCS
	-del /f /q *.obj *.bs Makefile *$(o) *.c pm_to_blib *.xdc *.err *.sym *.map *.def *.lib *.pdb
	cd ..\..\netware
	cd UCSExt
	-del /f /q *.obj *.bs Makefile *$(o) *.c pm_to_blib *.xdc *.err *.sym *.map *.c
	cd ..\..\netware
!endif
	-rmdir /s /q $(AUTODIR)
	-rmdir /s /q $(COREDIR)
	-del /f /q ..\config.sh

installwin:
	$(MINIPERL) -I..\lib ..\installperl

install : utils installwin perlimp

perlimp :
	copy perl.imp $(INST_COREDIR)

installnw:
	$(MINIPERL) -I..\lib ..\installperl -netware

install_tests :
	cd ..\t
	xcopy /f /r /i /s /d *.* $(INST_NW_TOP2)\scripts\t
	cd ..\lib
	xcopy /f /r /i /s /d *.t $(INST_NW_TOP2)\scripts\t\lib
	cd ..\ext
	xcopy /f /r /i /s /d *.t $(INST_NW_TOP2)\scripts\t\ext
	cd ..\netware\t
	xcopy /f /r /i /s /d *.pl $(INST_NW_TOP2)\scripts\t
	cd ..

nwinstall: utils installnw install_tests

inst_lib : $(CONFIGPM)
	copy ..\win32\splittree.pl .. 
	$(MINIPERL) -I..\lib ..\splittree.pl "../LIB" $(AUTODIR)
	$(RCOPY) ..\lib $(INST_LIB)\*.*

clean : 
	-@erase miniperlmain$(o)
	-@erase /f config.h
	-@erase $(DLL_OBJ)
	-@erase ..\*$(o) ..\*.lib ..\*.exp *$(o) *.lib *.exp *.res
	-@erase ..\t\*.exe ..\t\*.dll ..\t\*.bat
	-@erase ..\x2p\*.nlm ..\x2p\*.bat
