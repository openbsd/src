#	$OpenBSD: Makefile,v 1.8 2005/01/19 02:02:34 uwe Exp $


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
MANSUBDIR=i386 macppc
MLINKS=	apm.8 zzz.8

.include <bsd.prog.mk>
