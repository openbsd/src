#	$OpenBSD: Makefile,v 1.4 2000/05/18 16:46:37 espie Exp $

.if ${MACHINE} == "i386"
SRCS=	apmd.c apmsubr.c

CFLAGS+=	-g  -O2 -Wmissing-prototypes -Wall
PROG=	apmd
.else
NOPROG=yes
.endif

MAN=	apmd.8
MANSUBDIR=i386

.include <bsd.prog.mk>
