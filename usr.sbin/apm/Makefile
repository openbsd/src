#	$OpenBSD: Makefile,v 1.9 2005/02/21 21:59:40 uwe Exp $


.if (${MACHINE} == "i386") || (${MACHINE} == "macppc") || \
    (${MACHINE} == "zaurus")
SRCS=	apm.c apmsubr.c

.PATH:	${.CURDIR}/../apmd

CFLAGS+=	-I${.CURDIR}/../apmd
PROG=	apm
LINKS=	${BINDIR}/apm ${BINDIR}/zzz
.else
NOPROG=yes
.endif

MAN=	apm.8
MANSUBDIR=i386 macppc zaurus
MLINKS=	apm.8 zzz.8

.include <bsd.prog.mk>
