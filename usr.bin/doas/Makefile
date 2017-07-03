#	$OpenBSD: Makefile,v 1.3 2017/07/03 22:21:47 espie Exp $

SRCS=	parse.y doas.c env.c

PROG=	doas
MAN=	doas.1 doas.conf.5

BINOWN= root
BINMODE=4555

CFLAGS+= -I${.CURDIR}
COPTS+=	-Wall
YFLAGS=

.include <bsd.prog.mk>
