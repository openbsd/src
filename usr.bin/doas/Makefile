#	$OpenBSD: Makefile,v 1.1 2015/07/16 20:44:21 tedu Exp $

SRCS=	parse.y doas.c

PROG=	doas
MAN=	doas.1 doas.conf.5

BINOWN= root
BINMODE=4555

CFLAGS+= -I${.CURDIR}
COPTS+=	-Wall

.include <bsd.prog.mk>
