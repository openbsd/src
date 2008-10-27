#	$OpenBSD: Makefile,v 1.1 2008/10/27 00:26:33 ratchov Exp $

LIB=	sndio
MAN=	sio_open.3
SRCS=	aucat.c sun.c sndio.c
CFLAGS+=-Wall -Wstrict-prototypes -Werror -Wundef -DDEBUG \
	-I${.CURDIR} -I${.CURDIR}/../../usr.bin/aucat

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
	sio_open.3 sio_initpar.3

includes:
	@cd ${.CURDIR}; cmp -s sndio.h ${DESTDIR}/usr/include/sndio.h || \
		${INSTALL} ${INSTALL_COPY} -o $(BINOWN) -g $(BINGRP) \
		-m 444 sndio.h ${DESTDIR}/usr/include

.include <bsd.lib.mk>
