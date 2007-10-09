#	$OpenBSD: Makefile,v 1.2 2007/10/09 06:28:45 claudio Exp $


PROG=	ospf6d
SRCS=	area.c buffer.c carp.c control.c database.c hello.c \
	imsg.c interface.c iso_cksum.c kroute.c lsack.c \
	lsreq.c lsupdate.c log.c neighbor.c ospf6d.c ospfe.c packet.c \
	parse.y printconf.c rde.c rde_lsdb.c rde_spf.c name2id.c

MAN=	ospf6d.8 ospf6d.conf.5

CFLAGS+= -Wall -I${.CURDIR}
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
YFLAGS=
LDADD+=	-levent
DPADD+= ${LIBEVENT}

.include <bsd.prog.mk>
