#	$OpenBSD: Makefile,v 1.2 2005/02/02 21:22:28 norby Exp $


PROG=	ospfd
SRCS=	area.c auth.c buffer.c config.c control.c database.c hello.c \
	imsg.c in_cksum.c interface.c iso_cksum.c kroute.c lsack.c \
	lsreq.c lsupdate.c log.c neighbor.c ospfd.c ospfe.c packet.c \
	parse.y rde.c rde_lsdb.c

MAN=	ospfd.8 ospfd.conf.5

CFLAGS+= -Wall -I${.CURDIR}
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
YFLAGS=
LDADD+=	-levent

.include <bsd.prog.mk>
