#	$OpenBSD: Makefile,v 1.10 2013/05/14 20:30:47 miod Exp $

LIB=bug

NOPIC=
NOPROFILE=
NOPIE=

S=${.CURDIR}/../../../..
DIR_SA=$S/lib/libsa

CFLAGS+=-I${.CURDIR}/../../include -I${DIR_SA}

SRCS=	delay.c diskrd.c diskwr.c getbrdid.c inchr.c instat.c \
	netcfig.c netctrl.c netfopen.c netfread.c outstr.c putchar.c \
	return.c rtc_rd.c

install:

.include <bsd.lib.mk>
