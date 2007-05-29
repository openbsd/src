#	$OpenBSD: Makefile,v 1.7 2007/05/29 22:08:25 claudio Exp $


PROG=	ospfd
SRCS=	area.c auth.c buffer.c carp.c control.c database.c hello.c \
	imsg.c in_cksum.c interface.c iso_cksum.c kroute.c lsack.c \
	lsreq.c lsupdate.c log.c neighbor.c ospfd.c ospfe.c packet.c \
	parse.y printconf.c rde.c rde_lsdb.c rde_spf.c name2id.c

MAN=	ospfd.8 ospfd.conf.5

CFLAGS+= -Wall -I${.CURDIR}
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
YFLAGS=
LDADD+=	-levent
DPADD+= ${LIBEVENT}

.include <bsd.prog.mk>
