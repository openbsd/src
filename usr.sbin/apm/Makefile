#	$OpenBSD: Makefile,v 1.5 2000/05/18 16:46:36 espie Exp $


.if ${MACHINE} == "i386"
SRCS=	apm.c apmsubr.c

.PATH:	${.CURDIR}/../apmd

CFLAGS+=	-g  -O2 -Wmissing-prototypes -Wall -I${.CURDIR}/../apmd
PROG=	apm
LINKS=	${BINDIR}/apm ${BINDIR}/zzz
.else
NOPROG=yes
.endif

MAN=	apm.8
MANSUBDIR=i386
MLINKS=	apm.8 zzz.8

.include <bsd.prog.mk>
