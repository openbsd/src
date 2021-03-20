#	$OpenBSD: Makefile,v 1.3 2021/03/20 16:36:52 kn Exp $

PROG=	slaacd
COPTS+=	-DSMALL
.PATH:  ${.CURDIR}/../../../sbin/slaacd
SRCS=	engine.c frontend.c slaacd.c

MAN=

YFLAGS=
LDADD+=	-levent -lutil
DPADD+= ${LIBEVENT} ${LIBUTIL}

.include <bsd.prog.mk>
