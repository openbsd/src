#	$OpenBSD: Makefile,v 1.6 2008/12/01 20:22:56 todd Exp $

PROG=		btd
SRCS=		atomicio.c bt.c bt_subr.c btd.c conf.c control.c db.c \
		devinfo.c fdpass.c hci.c log.c sdp.c sdp_query.c \
		sdp_search.c util.c
MAN=		btd.8

LDADD+=		-levent -lusbhid
CPPFLAGS+=	-I${.CURDIR} -I${.CURDIR}/../btctl
COPTS+=		-Wall -Werror
DEBUG=		-g

.include <bsd.prog.mk>
