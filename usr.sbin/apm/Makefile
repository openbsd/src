# $Id: Makefile,v 1.3 1997/03/26 00:35:49 deraadt Exp $

SRCS=	apm.c apmsubr.c

.PATH:	${.CURDIR}/../apmd

CFLAGS+=	-g  -O2 -Wmissing-prototypes -Wall -I${.CURDIR}/../apmd
PROG=	apm
MAN=	apm.8
MLINKS=	apm.8 zzz.8
LINKS=	${BINDIR}/apm ${BINDIR}/zzz

.include <bsd.prog.mk>
