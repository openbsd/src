# $Id: Makefile,v 1.2 1996/11/14 15:04:51 mickey Exp $

SRCS=	apm.c apmsubr.c
#LDADD+=	-lutil

.PATH:	${.CURDIR}/../apmd

CFLAGS+=	-g  -O2 -Wmissing-prototypes -Wall -I${.CURDIR}/../apmd
PROG=	apm
MAN=	apm.8
MLINKS=	apm.8 zzz.8
LINKS=	${BINDIR}/apm ${BINDIR}/zzz

.include <bsd.prog.mk>
