#	$OpenBSD: Makefile,v 1.1 2006/10/18 16:11:58 norby Exp $

PROG=	ripd
SRCS=	auth.c buffer.c control.c imsg.c interface.c kroute.c \
	log.c message.c neighbor.c packet.c parse.y printconf.c \
	rde.c rde_rib.c ripe.c ripd.c

MAN=	ripd.8 ripd.conf.5

CFLAGS+= -Wall -I${.CURDIR}
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
YFLAGS=
LDADD+=	-levent

.include <bsd.prog.mk>
