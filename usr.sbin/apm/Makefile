#	$OpenBSD: Makefile,v 1.10 2005/03/29 16:26:45 miod Exp $


.if (${MACHINE} == "i386") || (${MACHINE} == "macppc") || \
    (${MACHINE} == "sparc") || (${MACHINE} == "zaurus")
SRCS=	apm.c apmsubr.c

.PATH:	${.CURDIR}/../apmd

CFLAGS+=	-I${.CURDIR}/../apmd
PROG=	apm
LINKS=	${BINDIR}/apm ${BINDIR}/zzz
.else
NOPROG=yes
.endif

MAN=	apm.8
MANSUBDIR=i386 macppc sparc zaurus
MLINKS=	apm.8 zzz.8

.include <bsd.prog.mk>
