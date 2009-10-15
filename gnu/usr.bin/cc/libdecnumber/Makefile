# $FreeBSD: src/gnu/usr.bin/cc/libdecnumber/Makefile,v 1.1.8.1 2009/04/15 03:14:26 kensmith Exp $
# $OpenBSD: Makefile,v 1.2 2009/10/15 22:29:42 robert Exp $

# Use our headers in preference to ones from ../cc_tools.
CFLAGS+=	-I${.CURDIR} -I.

.include <bsd.own.mk>

.include "${.CURDIR}/../Makefile.inc"

.PATH: ${GCCLIB}/libdecnumber

LIB=	decnumber
SRCS=	decNumber.c decContext.c decUtility.c \
        decimal32.c decimal64.c decimal128.c

NOPROFILE=	Yes
NOPIC=		Yes

install:
	# Nothing here so far...

.include <bsd.lib.mk>
