#	$OpenBSD: Makefile,v 1.9 2012/11/25 14:10:47 miod Exp $

LIB=bug

NOPIC=
NOPROFILE=
NOPIE=

S=${.CURDIR}/../../../..
DIR_SA=$S/lib/libsa

CFLAGS+=-I${.CURDIR}/../../include -I${DIR_SA}

SRCS=	delay.c diskrd.c diskwr.c getbrdid.c inchr.c instat.c \
	netcfig.c netctrl.c netfopen.c netfread.c outln.c outstr.c putchar.c \
	return.c rtc_rd.c

install:

.include <bsd.lib.mk>
