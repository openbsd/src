# $OpenBSD: Makefile,v 1.1 2006/11/03 19:33:56 marco Exp $

.if (${MACHINE} == "i386") || (${MACHINE} == "amd64")
PROG=	acpiscan
SRCS=	acpiscan.c oslib.c
.else
NOPROG=		yes
.endif

MAN=	acpiscan.8

CFLAGS+=-I${.CURDIR}
LDADD=-l${MACHINE}

BINDIR?=/usr/sbin

.include <bsd.prog.mk>
