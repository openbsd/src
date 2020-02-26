#	$OpenBSD: Makefile,v 1.14 2020/02/26 13:53:58 ratchov Exp $

LIB=	sndio
MAN=	sio_open.3 mio_open.3 sioctl_open.3 sndio.7
SRCS=	debug.c aucat.c sio_aucat.c sio_sun.c sio.c \
	mio_rmidi.c mio_aucat.c mio.c \
	sioctl_aucat.c sioctl_sun.c sioctl.c
CFLAGS+=-DDEBUG
COPTS+=	-Wall -Wstrict-prototypes -Wmissing-prototypes -Wpointer-arith -Wundef

VERSION_SCRIPT=	${.CURDIR}/Symbols.map

.include <bsd.lib.mk>
