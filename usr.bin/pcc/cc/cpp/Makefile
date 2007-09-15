#	$OpenBSD: Makefile,v 1.2 2007/09/15 21:41:00 otto Exp $
#
# Makefile for the cpp part of pcc.
#
PROG=   	cpp
PREFIX=		/usr/local
BINDIR=		${PREFIX}/libexec
MANDIR=		${PREFIX}/share/man/man
TARGOS=		openbsd
TARGMACH=	x86

CFLAGS+=	-DCPP_DEBUG -Wall -Wmissing-prototypes -Wstrict-prototypes -Werror
CFLAGS+=	-DLIBEXECDIR=\"${PREFIX}/libexec\"
CPPFLAGS+=	-I. -I${.CURDIR}

SRCS=cpy.y scanner.l cpp.c

.include <bsd.prog.mk>
