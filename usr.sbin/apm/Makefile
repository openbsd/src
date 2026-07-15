#	$OpenBSD: Makefile,v 1.21 2026/07/15 19:00:24 miod Exp $


.if ((${MACHINE} == "arm64") || (${MACHINE} == "amd64") || \
    (${MACHINE} == "i386") || \
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

.include <bsd.prog.mk>
