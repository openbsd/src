#	$OpenBSD: Makefile,v 1.7 2015/07/21 04:56:44 renato Exp $

PROG=	ldpd
SRCS=	accept.c adjacency.c address.c control.c hello.c init.c interface.c \
	keepalive.c kroute.c labelmapping.c lde.c lde_lib.c ldpd.c ldpe.c \
	log.c neighbor.c notification.c packet.c parse.y printconf.c pfkey.c \
	l2vpn.c

MAN=	ldpd.8 ldpd.conf.5

CFLAGS+= -Wall -I${.CURDIR}
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
YFLAGS=
LDADD+=	-levent -lutil
DPADD+= ${LIBEVENT} ${LIBUTIL}

.include <bsd.prog.mk>
