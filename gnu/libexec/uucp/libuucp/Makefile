# This is the Makefile for the libuucp subdirectory of Taylor UUCP
# $Id: Makefile,v 1.1.1.1 1995/10/18 08:38:38 deraadt Exp $

LIB=		uucp
SRCS=		buffer.c crc.c debug.c escape.c getopt.c getop1.c parse.c \
		spool.c status.c xfree.c xmall.c xreall.c getlin.c
CFLAGS+=	-I$(.CURDIR)/../common_sources

NOMAN=		noman
NOPROFILE=	noprofile
NOPIC=		nopic

install:

.include <bsd.lib.mk>
