#	$OpenBSD: Makefile,v 1.5 1996/05/16 02:25:39 chuck Exp $

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
