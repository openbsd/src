#	$OpenBSD: Makefile,v 1.6 2008/08/17 18:40:12 ragge Exp $
#
# Makefile for the cc part of pcc.
#
PROG=   	pcc
SRCS=		cc.c
MAN=		cc.1
PREFIX=		/usr/local
BINDIR=		${PREFIX}/bin
MANDIR=		${PREFIX}/man/man
TARGOS=		openbsd
TARGMACH=       ${MACHINE_ARCH}
#LINKS=		${PREFIX}/bin/pcc ${PREFIX}/bin/cc

CFLAGS+=	-DLIBEXECDIR=\"${PREFIX}/libexec/\"
CPPFLAGS+=	-I${.CURDIR}/..
CPPFLAGS+=	-DDEFMACH=\"${TARGMACH}\"

.include <bsd.prog.mk>
