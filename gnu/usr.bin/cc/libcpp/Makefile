# $FreeBSD: src/gnu/usr.bin/cc/libcpp/Makefile,v 1.1.8.1 2009/04/15 03:14:26 kensmith Exp $
# $OpenBSD: Makefile,v 1.2 2009/10/15 21:08:01 robert Exp $

# Use our headers in preference to ones from ../cc_tools.
CFLAGS+=	-I${.CURDIR} -I.

.include <bsd.own.mk>

.include "${.CURDIR}/../Makefile.inc"

.PATH: ${GCCLIB}/libcpp

LIB=	cpp
SRCS=	localedir.h
SRCS+=	charset.c directives.c errors.c expr.c files.c \
	identifiers.c init.c lex.c line-map.c macro.c mkdeps.c \
	pch.c symtab.c traditional.c

NOPROFILE=	Yes
NOPIC=		Yes

localedir.h:	Makefile
	echo	'#define LOCALEDIR "/usr/share/locale"' > localedir.h
SRCS+=		localedir.h
CLEANFILES+=	localedir.h

install:
	# Nothing here so far...

.include <bsd.lib.mk>
