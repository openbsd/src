#	$NetBSD: bsd.own.mk,v 1.23 1995/11/03 23:33:33 cgd Exp $

# Defining `SKEY' causes support for S/key authentication to be compiled in.
SKEY=		yes
# Defining `KERBEROS' causes support for Kerberos authentication to be
# compiled in.
#KERBEROS=	yes
# Defining 'KERBEROS5' causes support for Kerberos5 authentication to be
# compiled in.
#KERBEROS5=	yes

# where the system object and source trees are kept; can be configurable
# by the user in case they want them in ~/foosrc and ~/fooobj, for example
BSDSRCDIR?=	/usr/src
BSDOBJDIR?=	/usr/obj

BINGRP?=	bin
BINOWN?=	bin
BINMODE?=	555
NONBINMODE?=	444

# Define MANZ to have the man pages compressed (gzip)
#MANZ=		1

MANDIR?=	/usr/share/man/cat
MANGRP?=	bin
MANOWN?=	bin
MANMODE?=	${NONBINMODE}

LIBDIR?=	/usr/lib
LINTLIBDIR?=	/usr/libdata/lint
LIBGRP?=	${BINGRP}
LIBOWN?=	${BINOWN}
LIBMODE?=	${NONBINMODE}

DOCDIR?=        /usr/share/doc
DOCGRP?=	bin
DOCOWN?=	bin
DOCMODE?=       ${NONBINMODE}

NLSDIR?=	/usr/share/nls
NLSGRP?=	bin
NLSOWN?=	bin
NLSMODE?=	${NONBINMODE}

COPY?=		-c
STRIP?=		-s

# Define SYS_INCLUDE to indicate whether you want symbolic links to the system
# source (``symlinks''), or a separate copy (``copies''); (latter useful
# in environments where it's not possible to keep /sys publicly readable)
#SYS_INCLUDE= 	symlinks

# don't try to generate PIC versions of libraries on machines
# which don't support PIC.
.if (${MACHINE_ARCH} == "mips") || (${MACHINE_ARCH} == "alpha") || \
    (${MACHINE_ARCH} == "vax")
NOPIC=
.endif

# No lint, for now.
NOLINT=
