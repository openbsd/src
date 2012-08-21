#	$OpenBSD: Makefile,v 1.2 2012/08/21 14:46:18 pascal Exp $

LIB=prom

NOPIC=
NOPROFILE=
NOPIE=

S=${.CURDIR}/../../../..
DIR_SA=$S/lib/libsa

CFLAGS+=-I${.CURDIR}/../../include -I${DIR_SA}

SRCS=	getc.c getenaddr.c halt.c putc.c

install:

.include <bsd.lib.mk>
