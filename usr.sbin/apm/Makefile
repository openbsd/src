#	$OpenBSD: Makefile,v 1.7 2001/10/03 19:07:58 drahn Exp $


.if (${MACHINE} == "i386") || (${MACHINE} == "macppc")
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
