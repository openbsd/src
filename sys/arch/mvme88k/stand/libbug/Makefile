#	$OpenBSD: Makefile,v 1.7 2004/01/26 19:48:33 miod Exp $

LIB=bug

NOPIC=
NOPROFILE=

S=${.CURDIR}/../../../..
DIR_SA=$S/lib/libsa

CFLAGS+=-I${.CURDIR}/../../include -I${DIR_SA}

SRCS=	delay.c diskrd.c diskwr.c getbrdid.c inchr.c instat.c \
	netfopen.c netfread.c outln.c outstr.c putchar.c return.c rtc_rd.c

install:

.include <bsd.lib.mk>
