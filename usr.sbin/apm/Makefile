#	$OpenBSD: Makefile,v 1.6 2000/09/21 14:59:40 mickey Exp $


.if ${MACHINE} == "i386"
SRCS=	apm.c apmsubr.c

.PATH:	${.CURDIR}/../apmd

CFLAGS+=	-I${.CURDIR}/../apmd
PROG=	apm
LINKS=	${BINDIR}/apm ${BINDIR}/zzz
.else
NOPROG=yes
.endif

MAN=	apm.8
MANSUBDIR=i386
MLINKS=	apm.8 zzz.8

.include <bsd.prog.mk>
