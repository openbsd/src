#	$OpenBSD: Makefile,v 1.2 1996/04/28 10:48:33 deraadt Exp $

LIB=bug

NOPIC=
NOPROFILE=

CFLAGS+=-I${.CURDIR}/../../include

SRCS=delay.c diskrd.c diskwr.c getbrdid.c instat.c outln.c outstr.c \
	return.c rtc_rd.c
.PATH:    ${.CURDIR}/../../../../lib/libc_sa ${.CURDIR}/${MACHINE_ARCH}

install:

.include <bsd.lib.mk>
