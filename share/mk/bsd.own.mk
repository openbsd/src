#	$OpenBSD: bsd.own.mk,v 1.156 2014/12/28 05:17:21 deraadt Exp $
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
# Set `YP' to `yes' to build with support for NIS/YP.
YP?=		yes
# Set `DEBUGLIBS' to `yes' to build libraries with debugging symbols
DEBUGLIBS?=	no

GCC3_ARCH=m88k vax
BINUTILS217_ARCH=hppa64 ia64

# arm needs binutils-2.17, which still lacks W^X support
# sparc has not been tried
# m88k unknown
# hppa64 unknown
PIE_ARCH=alpha amd64 hppa i386 mips64 mips64el powerpc sh sparc64
STATICPIE_ARCH=alpha amd64 hppa i386 powerpc sparc64

.for _arch in ${MACHINE_ARCH}
.if !empty(GCC3_ARCH:M${_arch})
COMPILER_VERSION?=gcc3
.else
COMPILER_VERSION?=gcc4
.endif

.if !empty(BINUTILS217_ARCH:M${_arch})
BINUTILS_VERSION=binutils-2.17
.else
BINUTILS_VERSION=binutils
.endif

.if !empty(STATICPIE_ARCH:M${_arch})
STATICPIE?=-pie
.endif

.if !empty(PIE_ARCH:M${_arch})
NOPIE_FLAGS?=-fno-pie
NOPIE_LDFLAGS?=-nopie
PIE_DEFAULT?=${DEFAULT_PIE_DEF}
.else
NOPIE_FLAGS?=
PIE_DEFAULT?=
.endif
.endfor

.if ${COMPILER_VERSION} == "gcc4"
VISIBILITY_HIDDEN?=-fvisibility=hidden
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

SHAREDIR?=	/usr/share
SHAREGRP?=	bin
SHAREOWN?=	root
SHAREMODE?=	${NONBINMODE}

MANDIR?=	/usr/share/man/man
MANGRP?=	bin
MANOWN?=	root
MANMODE?=	${NONBINMODE}

LIBDIR?=	/usr/lib
LIBGRP?=	${BINGRP}
LIBOWN?=	${BINOWN}
LIBMODE?=	${NONBINMODE}

DOCDIR?=	/usr/share/doc
DOCGRP?=	bin
DOCOWN?=	root
DOCMODE?=	${NONBINMODE}

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

.if !defined(CDIAGFLAGS)
CDIAGFLAGS=	-Wall -Wpointer-arith -Wuninitialized -Wstrict-prototypes
CDIAGFLAGS+=	-Wmissing-prototypes -Wunused -Wsign-compare
CDIAGFLAGS+=	-Wshadow
.  if ${COMPILER_VERSION} == "gcc4"
CDIAGFLAGS+=	-Wdeclaration-after-statement
.  endif
.endif

# Shared files for system gnu configure, not used yet
GNUSYSTEM_AUX_DIR?=${BSDSRCDIR}/share/gnu

INSTALL_COPY?=	-c
.ifndef DEBUG
INSTALL_STRIP?=	-s
.endif

STATIC?=	-static ${STATICPIE}

# Define SYS_INCLUDE to indicate whether you want symbolic links to the system
# source (``symlinks''), or a separate copy (``copies''); (latter useful
# in environments where it's not possible to keep /sys publicly readable)
#SYS_INCLUDE= 	symlinks

# don't try to generate PIC versions of libraries on machines
# which don't support PIC.
.if ${MACHINE_ARCH} == "vax"
NOPIC=
.endif

# pic relocation flags.
.if (${MACHINE_ARCH} == "alpha") || (${MACHINE_ARCH} == "sparc64")
PICFLAG?=-fPIC
.else
PICFLAG?=-fpic
.endif

.if ${MACHINE_ARCH} == "sparc" || ${MACHINE_ARCH} == "sparc64"
ASPICFLAG=-KPIC
.endif

.if ${MACHINE_ARCH} == "alpha" || ${MACHINE_ARCH} == "powerpc" || \
    ${MACHINE_ARCH} == "sparc64"
# big PIE
DEFAULT_PIE_DEF=-DPIE_DEFAULT=2
.else
# small pie
DEFAULT_PIE_DEF=-DPIE_DEFAULT=1
.endif

# don't try to generate PROFILED versions of libraries on machines
# which don't support profiling.
.if 0
NOPROFILE=
.endif

BSD_OWN_MK=Done

.PHONY: spell clean cleandir obj manpages print all \
	depend beforedepend afterdepend cleandepend subdirdepend \
	all cleanman nlsinstall cleannls includes \
	beforeinstall realinstall maninstall afterinstall install
