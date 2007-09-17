#	$OpenBSD: Makefile,v 1.4 2007/09/17 07:20:49 todd Exp $
#
# Makefile for the cc part of pcc.
#
PROG=   	cc
PREFIX=		/usr/local
BINDIR=		${PREFIX}/bin
MANDIR=		${PREFIX}/man/man
TARGOS=		openbsd
TARGMACH=	x86

CFLAGS+=	-DLIBEXECDIR=\"${PREFIX}/libexec\"
CPPFLAGS+=	-I${.CURDIR}/../.. -I${.CURDIR}/../../os/${TARGOS}
CPPFLAGS+=	-Dmach_${TARGMACH}

.include <bsd.prog.mk>
