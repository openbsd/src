#	$OpenBSD: Makefile,v 1.4 1997/09/21 11:43:16 deraadt Exp $

SRCS=	apm.c apmsubr.c

.PATH:	${.CURDIR}/../apmd

CFLAGS+=	-g  -O2 -Wmissing-prototypes -Wall -I${.CURDIR}/../apmd
PROG=	apm
MAN=	apm.8
MLINKS=	apm.8 zzz.8
LINKS=	${BINDIR}/apm ${BINDIR}/zzz

.include <bsd.prog.mk>
