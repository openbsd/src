#	$OpenBSD: Makefile,v 1.4 1996/05/10 20:03:48 deraadt Exp $

LIB=bug

NOPIC=
NOPROFILE=

CLEANFILES+=machine

CFLAGS+=-I${.OBJDIR}

SRCS=	delay.c diskrd.c diskwr.c getbrdid.c instat.c outln.c outstr.c \
	return.c rtc_rd.c
.PATH:    ${.CURDIR}/../../../../lib/libc_sa

all:	machine libbug.a

install:

machine:
	rm -f machine; ln -s ${.CURDIR}/../../include machine

.include <bsd.lib.mk>
