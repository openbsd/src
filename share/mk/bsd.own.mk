#	$OpenBSD: bsd.own.mk,v 1.103 2010/07/08 20:25:40 kettenis Exp $
#	$NetBSD: bsd.own.mk,v 1.24 1996/04/13 02:08:09 thorpej Exp $

# Host-specific overrides
.if defined(MAKECONF) && exists(${MAKECONF})
.include "${MAKECONF}"
.elif exists(/etc/mk.conf)
.include "/etc/mk.conf"
.endif

# Set `WARNINGS' to `yes' to add appropriate warnings to each compilation
WARNINGS?=	no
# Set `SKEY' to `yes' to build with support for S/key authentication.
SKEY?=		yes
# Set `KERBEROS5' to `yes' to build with support for Kerberos5 authentication.
KERBEROS5?=	yes
# Set `YP' to `yes' to build with support for NIS/YP.
YP?=		yes
# Set `TCP_WRAPPERS' to `yes' to build certain networking daemons with
# integrated support for libwrap.
TCP_WRAPPERS?=	yes
# Set `AFS` to `yes' to build with AFS support.
AFS?=		yes
# Set `DEBUGLIBS' to `yes' to build libraries with debugging symbols
DEBUGLIBS?=	no
# Set toolchain to be able to know differences.
.if ${MACHINE_ARCH} == "m68k" || ${MACHINE_ARCH} == "m88k" || \
    ${MACHINE_ARCH} == "vax"
ELF_TOOLCHAIN?=	no
.else
ELF_TOOLCHAIN?=	yes
.endif

# gcc3
.if ${MACHINE_ARCH} == "m68k" || ${MACHINE_ARCH} == "m88k" || \
    ${MACHINE_ARCH} == "sparc" || ${MACHINE_ARCH} == "vax"
USE_GCC3?=no
COMPILER_VERSION?=gcc2
.elif ${MACHINE_ARCH} == "amd64" || ${MACHINE_ARCH} == "hppa" || \
    ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "powerpc" || \
    ${MACHINE_ARCH} == "sparc64"
COMPILER_VERSION?=gcc4
.else
USE_GCC3?=yes
COMPILER_VERSION?=gcc3
.endif

# where the system object and source trees are kept; can be configurable
# by the user in case they want them in ~/foosrc and ~/fooobj, for example
BSDSRCDIR?=	/usr/src
BSDOBJDIR?=	/usr/obj

BINGRP?=	bin
BINOWN?=	root
BINMODE?=	555
NONBINMODE?=	444
DIRMODE?=	755

# Define MANZ to have the man pages compressed (gzip)
#MANZ=		1

# Define MANPS to have PostScript manual pages generated
#MANPS=		1

SHAREDIR?=	/usr/share
SHAREGRP?=	bin
SHAREOWN?=	root
SHAREMODE?=	${NONBINMODE}

MANDIR?=	/usr/share/man/cat
MANGRP?=	bin
MANOWN?=	root
MANMODE?=	${NONBINMODE}

PSDIR?=		/usr/share/man/ps
PSGRP?=		bin
PSOWN?=		root
PSMODE?=	${NONBINMODE}

LIBDIR?=	/usr/lib
LINTLIBDIR?=	/usr/libdata/lint
LIBGRP?=	${BINGRP}
LIBOWN?=	${BINOWN}
LIBMODE?=	${NONBINMODE}

DOCDIR?=        /usr/share/doc
DOCGRP?=	bin
DOCOWN?=	root
DOCMODE?=       ${NONBINMODE}

LKMDIR?=	/usr/lkm
LKMGRP?=	${BINGRP}
LKMOWN?=	${BINOWN}
LKMMODE?=	${NONBINMODE}

NLSDIR?=	/usr/share/nls
NLSGRP?=	bin
NLSOWN?=	root
NLSMODE?=	${NONBINMODE}

LOCALEDIR?=	/usr/share/locale
LOCALEGRP?=	wheel
LOCALEOWN?=	root
LOCALEMODE?=	${NONBINMODE}

# Shared files for system gnu configure, not used yet
GNUSYSTEM_AUX_DIR?=${BSDSRCDIR}/share/gnu

INSTALL_COPY?=	-c
.ifndef DEBUG
INSTALL_STRIP?=	-s
.endif

# This may be changed for _single filesystem_ configurations (such as
# routers and other embedded systems); normal systems should leave it alone!
STATIC?=	-static

# Define SYS_INCLUDE to indicate whether you want symbolic links to the system
# source (``symlinks''), or a separate copy (``copies''); (latter useful
# in environments where it's not possible to keep /sys publicly readable)
#SYS_INCLUDE= 	symlinks

# don't try to generate PIC versions of libraries on machines
# which don't support PIC.
.if ${MACHINE_ARCH} == "m88k" || ${MACHINE_ARCH} == "vax"
NOPIC=
.endif

# pic relocation flags.
.if (${MACHINE_ARCH} == "alpha") || (${MACHINE_ARCH} == "sparc64")
PICFLAG?=-fPIC
.else
PICFLAG?=-fpic
. if ${MACHINE_ARCH} == "m68k"
# Function CSE makes gas -k not recognize external function calls as lazily
# resolvable symbols, thus sometimes making ld.so report undefined symbol
# errors on symbols found in shared library members that would never be
# called.  Ask niklas@openbsd.org for details.
PICFLAG+=-fno-function-cse
. endif
.endif

.if ${MACHINE_ARCH} == "sparc" || ${MACHINE_ARCH} == "sparc64"
ASPICFLAG=-KPIC
.elif ${ELF_TOOLCHAIN:L} == "no"
ASPICFLAG=-k
.endif

# don't try to generate PROFILED versions of libraries on machines
# which don't support profiling.
.if 0
NOPROFILE=
.endif

BSD_OWN_MK=Done

.PHONY: spell clean cleandir obj manpages print all \
	depend beforedepend afterdepend cleandepend subdirdepend \
	all lint cleanman nlsinstall cleannls includes \
	beforeinstall realinstall maninstall afterinstall install
