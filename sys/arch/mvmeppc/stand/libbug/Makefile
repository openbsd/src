#	$OpenBSD: Makefile,v 1.1 2001/06/26 21:58:03 smurph Exp $

LIB=bug

NOPIC=
NOPROFILE=

S=${.CURDIR}/../../../..
DIR_SA=$S/lib/libsa

CFLAGS+=-O2 -I${.CURDIR}/../../include -I${DIR_SA}

SRCS=	delay.c diskrd.c diskwr.c getbrdid.c inchr.c instat.c outln.c outch.c\
	outstr.c putchar.c return.c rtc_rd.c netrd.c netwr.c netfopen.c \
	netfread.c netctrl.c

install:

.include <bsd.lib.mk>
