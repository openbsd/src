#	$OpenBSD: Makefile,v 1.4 2008/12/17 14:19:39 michele Exp $

PROG=	ripd
SRCS=	auth.c buffer.c carp.c control.c imsg.c interface.c kroute.c \
	log.c message.c name2id.c neighbor.c packet.c parse.y \
	printconf.c rde.c rde_rib.c ripe.c ripd.c

MAN=	ripd.8 ripd.conf.5

CFLAGS+= -Wall -I${.CURDIR}
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
YFLAGS=
LDADD+=	-levent
DPADD+= ${LIBEVENT}

.include <bsd.prog.mk>
