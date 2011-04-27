#	$OpenBSD: Makefile,v 1.8 2011/04/27 21:20:36 ratchov Exp $

LIB=	sndio
MAN=	sio_open.3 mio_open.3 sndio.7
SRCS=	debug.c aucat.c sio_aucat.c sio_sun.c sio.c \
	mio_rmidi.c mio_aucat.c mio.c
CFLAGS+=-Wall -Wstrict-prototypes -Werror -Wundef -DDEBUG

MLINKS = \
	sio_open.3 sio_close.3 \
	sio_open.3 sio_setpar.3 \
	sio_open.3 sio_getpar.3 \
	sio_open.3 sio_getcap.3 \
	sio_open.3 sio_start.3 \
	sio_open.3 sio_stop.3 \
	sio_open.3 sio_read.3 \
	sio_open.3 sio_write.3 \
	sio_open.3 sio_onmove.3 \
	sio_open.3 sio_nfds.3 \
	sio_open.3 sio_pollfd.3 \
	sio_open.3 sio_revents.3 \
	sio_open.3 sio_eof.3 \
	sio_open.3 sio_setvol.3 \
	sio_open.3 sio_onvol.3 \
	sio_open.3 sio_initpar.3 \
	mio_open.3 mio_close.3 \
	mio_open.3 mio_read.3 \
	mio_open.3 mio_write.3 \
	mio_open.3 mio_nfds.3 \
	mio_open.3 mio_pollfd.3 \
	mio_open.3 mio_revents.3 \
	mio_open.3 mio_eof.3

.include <bsd.lib.mk>
