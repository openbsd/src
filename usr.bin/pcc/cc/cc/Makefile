#	$OpenBSD: Makefile,v 1.5 2007/09/19 15:21:07 todd Exp $
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
CPPFLAGS+=	-DDEFMACH=\"${TARGMACH}\"

.include <bsd.prog.mk>
