#	$OpenBSD: Makefile,v 1.13 2017/12/26 15:23:33 jca Exp $

LIB=	sndio
MAN=	sio_open.3 mio_open.3 sndio.7
SRCS=	debug.c aucat.c sio_aucat.c sio_sun.c sio.c \
	mio_rmidi.c mio_aucat.c mio.c
CFLAGS+=-DDEBUG
COPTS+=	-Wall -Wstrict-prototypes -Wmissing-prototypes -Wpointer-arith -Wundef

VERSION_SCRIPT=	${.CURDIR}/Symbols.map

.include <bsd.lib.mk>
