#	$OpenBSD: Makefile,v 1.12 2008/08/11 18:45:13 kettenis Exp $


.if (${MACHINE} == "amd64") || (${MACHINE} == "i386") || \
    (${MACHINE} == "macppc") || (${MACHINE} == "sparc") || \
    (${MACHINE} == "sparc64") || (${MACHINE} == "zaurus")
SRCS=	apm.c apmsubr.c

.PATH:	${.CURDIR}/../apmd

CFLAGS+=	-I${.CURDIR}/../apmd
PROG=	apm
LINKS=	${BINDIR}/apm ${BINDIR}/zzz
.else
NOPROG=yes
.endif

MAN=	apm.8
MANSUBDIR=amd64 i386 macppc sparc sparc64 zaurus
MLINKS=	apm.8 zzz.8

.include <bsd.prog.mk>
