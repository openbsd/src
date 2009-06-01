#	$OpenBSD: Makefile,v 1.1 2009/06/01 20:59:45 michele Exp $

PROG=	ldpd
SRCS=	address.c buffer.c control.c hello.c imsg.c init.c interface.c \
	keepalive.c kroute.c labelmapping.c lde.c lde_lib.c ldpd.c ldpe.c \
	log.c name2id.c neighbor.c notification.c packet.c parse.y printconf.c

MAN=	ldpd.8 ldpd.conf.5

CFLAGS+= -Wall -I${.CURDIR}
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
YFLAGS=
LDADD+=	-levent
DPADD+= ${LIBEVENT}

.include <bsd.prog.mk>
