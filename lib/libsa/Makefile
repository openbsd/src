#	$OpenBSD: Makefile,v 1.2 2008/10/26 13:55:03 ratchov Exp $

LIB=	sa
MAN=	sa_open.3
SRCS=	aucat.c sun.c libsa.c
CFLAGS+=-Wall -Wstrict-prototypes -Werror -Wundef -DDEBUG \
	-I${.CURDIR} -I${.CURDIR}/../../usr.bin/aucat

MLINKS = \
	sa_open.3 sa_close.3 \
	sa_open.3 sa_setpar.3 \
	sa_open.3 sa_getpar.3 \
	sa_open.3 sa_getcap.3 \
	sa_open.3 sa_start.3 \
	sa_open.3 sa_stop.3 \
	sa_open.3 sa_read.3 \
	sa_open.3 sa_write.3 \
	sa_open.3 sa_onmove.3 \
	sa_open.3 sa_nfds.3 \
	sa_open.3 sa_pollfd.3 \
	sa_open.3 sa_revents.3 \
	sa_open.3 sa_eof.3 \
	sa_open.3 sa_initpar.3

includes:
	@cd ${.CURDIR}; cmp -s libsa.h ${DESTDIR}/usr/include/libsa.h || \
		${INSTALL} ${INSTALL_COPY} -o $(BINOWN) -g $(BINGRP) \
		-m 444 libsa.h ${DESTDIR}/usr/include

.include <bsd.lib.mk>
