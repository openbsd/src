#	$OpenBSD: Makefile,v 1.6 2003/10/01 20:39:41 miod Exp $

LIB=bug

NOPIC=
NOPROFILE=

S=${.CURDIR}/../../../..
DIR_SA=$S/lib/libsa

CFLAGS+=-I${.CURDIR}/../../include -I${DIR_SA}

SRCS=	delay.c diskrd.c diskwr.c getbrdid.c inchr.c instat.c outln.c \
	outstr.c putchar.c return.c rtc_rd.c

install:

.include <bsd.lib.mk>
