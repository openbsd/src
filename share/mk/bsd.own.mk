#	$OpenBSD: bsd.own.mk,v 1.28 2000/02/08 20:26:29 espie Exp $
#	$NetBSD: bsd.own.mk,v 1.24 1996/04/13 02:08:09 thorpej Exp $

# Host-specific overrides
.if defined(MAKECONF) && exists(${MAKECONF})
.include "${MAKECONF}"
.elif exists(/etc/mk.conf)
.include "/etc/mk.conf"
.endif

# XXX - This is temporary until everyone uses UVM
.if (${MACHINE_ARCH} == "sparc") || (${MACHINE_ARCH} == "i386")
UVM?=		yes
.else
UVM?=		no
.endif

# Set `SKEY' to `yes' to build with support for S/key authentication.
SKEY?=		yes
# Set `KERBEROS' to `yes' to build with support for Kerberos authentication.
KERBEROS?=	yes
# Set `KERBEROS5' to `yes' to build with support for Kerberos5 authentication.
KERBEROS5?=	no
# Set `YP' to `yes' to build with support for NIS/YP.
YP?=		yes
# Set `TCP_WRAPPERS' to `yes' to build certain networking daemons with
# integrated support for libwrap.
TCP_WRAPPERS?=	yes
# Set `AFS` to `yes' to build with AFS support.
.if (${MACHINE_ARCH} == "m88k")
AFS?=		no
.else
AFS?=		yes
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

SHAREDIR?=	/usr/share
SHAREGRP?=	bin
SHAREOWN?=	root
SHAREMODE?=	${NONBINMODE}

MANDIR?=	/usr/share/man/cat
MANGRP?=	bin
MANOWN?=	root
MANMODE?=	${NONBINMODE}

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
.if (${MACHINE_ARCH} == "alpha") || (${MACHINE_ARCH} == "powerpc") || \
    (${MACHINE_ARCH} == "vax") || (${MACHINE_ARCH} == "hppa") || \
    (${MACHINE_ARCH} == "m88k")
NOPIC=
.endif

# don't try to generate PROFILED versions of libraries on machines
# which don't support profiling.
# to add this back use the following line
.if (${MACHINE_ARCH} == "m88k")
#.if 0
NOPROFILE=
.endif

# No lint, for now.
NOLINT=
