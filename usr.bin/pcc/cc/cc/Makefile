#	$OpenBSD: Makefile,v 1.3 2007/09/16 19:16:58 kjell Exp $
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
CPPFLAGS+=	-Dos_${TARGOS} -Dmach_${TARGMACH}

.include <bsd.prog.mk>
