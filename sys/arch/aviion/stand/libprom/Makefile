#	$OpenBSD: Makefile,v 1.1 2006/05/16 22:48:18 miod Exp $

LIB=prom

NOPIC=
NOPROFILE=

S=${.CURDIR}/../../../..
DIR_SA=$S/lib/libsa

CFLAGS+=-I${.CURDIR}/../../include -I${DIR_SA}

SRCS=	getc.c getenaddr.c halt.c putc.c

install:

.include <bsd.lib.mk>
