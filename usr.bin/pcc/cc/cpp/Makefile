#	$OpenBSD: Makefile,v 1.3 2007/09/16 19:16:59 kjell Exp $
#
# Makefile for the cpp part of pcc.
#
PROG=   	cpp
PREFIX=		/usr/local
BINDIR=		${PREFIX}/libexec
MANDIR=		${PREFIX}/man/man
TARGOS=		openbsd
TARGMACH=	x86

CFLAGS+=	-DCPP_DEBUG -Wall -Wmissing-prototypes -Wstrict-prototypes -Werror
CFLAGS+=	-DLIBEXECDIR=\"${PREFIX}/libexec\"
CPPFLAGS+=	-I. -I${.CURDIR}

SRCS=cpy.y scanner.l cpp.c

.include <bsd.prog.mk>
