#	$OpenBSD: Makefile,v 1.3 2013/10/16 16:59:35 miod Exp $

LIB=prom

NOPIC=
NOPROFILE=
NOPIE=

S=${.CURDIR}/../../../..
DIR_SA=$S/lib/libsa

CFLAGS+=-I${.CURDIR}/../../include -I${DIR_SA}

SRCS=	cpuid.c getc.c getenaddr.c halt.c putc.c

install:

.include <bsd.lib.mk>
