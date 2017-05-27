#	$OpenBSD: Makefile,v 1.2 2017/05/27 16:16:49 florian Exp $

PROG=	slaacd
SRCS=	control.c engine.c frontend.c log.c slaacd.c

MAN=	slaacd.8

#DEBUG=	-g -DDEBUG=3 -O0

CFLAGS+= -DSKIP_PROPOSAL

CFLAGS+= -Wall -I${.CURDIR}
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
YFLAGS=
LDADD+=	-levent -lutil
DPADD+= ${LIBEVENT} ${LIBUTIL}

.include <bsd.prog.mk>
