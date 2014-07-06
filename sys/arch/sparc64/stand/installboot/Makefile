#	$OpenBSD: Makefile,v 1.9 2014/07/06 12:13:18 miod Exp $
#	$NetBSD: Makefile,v 1.13 2000/08/16 08:24:01 mrg Exp $

.include <bsd.own.mk>

MAN=	installboot.8
MANSUBDIR=sparc64

.if ${MACHINE} == "sparc64"
PROG=	installboot
SRCS=	installboot.c
LDADD=	-lutil
DPADD=	${LIBUTIL}
CFLAGS+= -Wall
.else
NOPROG=
.endif

LDSTATIC?=	-static

STANDALONE=

.include <bsd.prog.mk>
