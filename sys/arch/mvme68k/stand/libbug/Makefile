#	$OpenBSD: Makefile,v 1.3 1996/05/07 11:25:05 deraadt Exp $

LIB=bug

NOPIC=
NOPROFILE=

CFLAGS+=-I${.CURDIR}/../../include

SRCS=	delay.c diskrd.c diskwr.c getbrdid.c instat.c outln.c outstr.c \
	return.c rtc_rd.c
.PATH:    ${.CURDIR}/../../../../lib/libc_sa

install:

.include <bsd.lib.mk>
