#	$OpenBSD: Makefile,v 1.14 2012/03/26 20:17:43 deraadt Exp $


.if (${MACHINE} == "amd64") || (${MACHINE} == "i386") || \
    (${MACHINE} == "loongson") || (${MACHINE} == "macppc") || \
    (${MACHINE} == "sparc") || (${MACHINE} == "sparc64") || \
    (${MACHINE} == "zaurus")

SRCS=	apm.c apmsubr.c

.PATH:	${.CURDIR}/../apmd

CFLAGS+=	-I${.CURDIR}/../apmd
PROG=	apm
LINKS=	${BINDIR}/apm ${BINDIR}/zzz
LINKS+=	${BINDIR}/apm ${BINDIR}/ZZZ
.else
NOPROG=yes
.endif

MAN=	apm.8
MANSUBDIR=amd64 i386 loongson macppc sparc sparc64 zaurus
MLINKS=	apm.8 zzz.8 apm.8 ZZZ.8

.include <bsd.prog.mk>
