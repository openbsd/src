#	$OpenBSD: Makefile,v 1.1 2008/11/24 23:34:41 uwe Exp $

PROG=		btd
SRCS=		bt.c btd.c conf.c control.c db.c devinfo.c \
		hci.c log.c sdp.c util.c
NOMAN=

LDADD+=		-levent -lbluetooth -lsdp -lusbhid
LDFLAGS+=	-L/usr/local/lib
CPPFLAGS+=	-I${.CURDIR} -I${.CURDIR}/../btctl -I/usr/local/include
COPTS+=		-Wall -Werror
DEBUG=		-g

.include <bsd.prog.mk>
