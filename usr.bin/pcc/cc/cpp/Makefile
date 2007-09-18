#	$OpenBSD: Makefile,v 1.4 2007/09/18 21:06:43 todd Exp $
#
# Makefile for the cpp part of pcc.
#
PROG=   	cpp
PREFIX=		/usr/local
BINDIR=		${PREFIX}/libexec
MANDIR=		${PREFIX}/man/man
TARGOS=		openbsd

CFLAGS+=	-DCPP_DEBUG -Wall -Wmissing-prototypes -Wstrict-prototypes -Werror
CFLAGS+=	-DLIBEXECDIR=\"${PREFIX}/libexec\"
CPPFLAGS+=	-I. -I${.CURDIR}

SRCS=cpy.y scanner.l cpp.c

.include <bsd.prog.mk>
