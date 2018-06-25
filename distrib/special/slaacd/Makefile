#	$OpenBSD: Makefile,v 1.2 2018/06/25 16:29:00 deraadt Exp $

PROG=	slaacd
COPTS+=	-DSMALL
.PATH:  ${.CURDIR}/../../../sbin/slaacd
SRCS=	engine.c frontend.c slaacd.c

MAN=

CFLAGS+= -DSKIP_PROPOSAL

YFLAGS=
LDADD+=	-levent -lutil
DPADD+= ${LIBEVENT} ${LIBUTIL}

.include <bsd.prog.mk>
