#	$OpenBSD: Makefile,v 1.1 2005/01/28 14:05:40 claudio Exp $


PROG=	ospfd
SRCS=	area.c auth.c buffer.c config.c control.c database.c hello.c \
	imsg.c in_cksum.c interface.c iso_cksum.c kroute.c lsack.c \
	lsreq.c lsupdate.c log.c neighbor.c ospfd.c ospfe.c packet.c \
	parse.y rde.c rde_lsdb.c

MAN=	ospfd.8

CFLAGS+= -Wall -I${.CURDIR}
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
YFLAGS=
LDADD+=	-levent

.include <bsd.prog.mk>
