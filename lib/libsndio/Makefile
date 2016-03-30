#	$OpenBSD: Makefile,v 1.12 2016/03/30 06:38:43 jmc Exp $

LIB=	sndio
MAN=	sio_open.3 mio_open.3 sndio.7
SRCS=	debug.c aucat.c sio_aucat.c sio_sun.c sio.c \
	mio_rmidi.c mio_aucat.c mio.c
CFLAGS+=-DDEBUG
COPTS+=	-Wall -Wstrict-prototypes -Wmissing-prototypes -Wpointer-arith -Wundef

.include <bsd.lib.mk>
