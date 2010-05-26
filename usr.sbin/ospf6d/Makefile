#	$OpenBSD: Makefile,v 1.3 2010/05/26 16:44:32 nicm Exp $

PROG=	ospf6d
SRCS=	area.c carp.c control.c database.c hello.c \
	interface.c iso_cksum.c kroute.c lsack.c \
	lsreq.c lsupdate.c log.c neighbor.c ospf6d.c ospfe.c packet.c \
	parse.y printconf.c rde.c rde_lsdb.c rde_spf.c name2id.c

MAN=	ospf6d.8 ospf6d.conf.5

CFLAGS+= -Wall -I${.CURDIR}
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
YFLAGS=
LDADD+=	-levent -lutil
DPADD+= ${LIBEVENT} ${LIBUTIL}

.include <bsd.prog.mk>
