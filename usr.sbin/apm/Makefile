#	$OpenBSD: Makefile,v 1.16 2016/09/01 09:59:40 deraadt Exp $


.if (${MACHINE} == "amd64") || (${MACHINE} == "i386") || \
    (${MACHINE} == "loongson") || (${MACHINE} == "macppc") || \
    (${MACHINE} == "sparc64") || (${MACHINE} == "zaurus")

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
MANSUBDIR=amd64 i386 loongson macppc sparc64 zaurus

.include <bsd.prog.mk>
