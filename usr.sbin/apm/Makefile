#	$OpenBSD: Makefile,v 1.19 2020/07/15 22:46:52 deraadt Exp $


.if ((${MACHINE} == "arm64") || (${MACHINE} == "amd64") || \
    (${MACHINE} == "i386") || (${MACHINE} == "loongson") || \
    (${MACHINE} == "macppc") || (${MACHINE} == "sparc64")) || \
    (${MACHINE} == "powerpc64")

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
MANSUBDIR=arm64 amd64 i386 loongson macppc sparc64

.include <bsd.prog.mk>
