#	$OpenBSD: Makefile,v 1.1 2017/07/06 15:05:28 florian Exp $

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
