#	$OpenBSD: Makefile,v 1.2 2016/06/16 17:40:30 tedu Exp $

SRCS=	parse.y doas.c env.c

PROG=	doas
MAN=	doas.1 doas.conf.5

BINOWN= root
BINMODE=4555

CFLAGS+= -I${.CURDIR}
COPTS+=	-Wall

.include <bsd.prog.mk>
