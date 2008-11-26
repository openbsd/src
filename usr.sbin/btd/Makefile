#	$OpenBSD: Makefile,v 1.4 2008/11/26 15:32:56 uwe Exp $

PROG=		btd
SRCS=		atomicio.c bt.c bt_subr.c btd.c conf.c control.c db.c \
		devinfo.c fdpass.c hci.c log.c sdp.c util.c
NOMAN=

LDADD+=		-levent -lusbhid
CPPFLAGS+=	-I${.CURDIR} -I${.CURDIR}/../btctl
COPTS+=		-Wall -Werror
DEBUG=		-g

.include <bsd.prog.mk>
