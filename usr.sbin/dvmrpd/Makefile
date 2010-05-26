#	$OpenBSD: Makefile,v 1.4 2010/05/26 16:44:32 nicm Exp $

PROG=	dvmrpd
SRCS=	ask_nbrs2.c control.c dvmrpd.c dvmrpe.c graft.c graft_ack.c \
	group.c igmp.c interface.c in_cksum.c kmroute.c kroute.c log.c \
	nbrs2.c neighbor.c packet.c parse.y printconf.c probe.c prune.c rde.c \
	rde_mfc.c rde_srt.c report.c

MAN=	dvmrpd.8 dvmrpd.conf.5

CFLAGS+= -Wall -I${.CURDIR}
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
YFLAGS=
LDADD+=	-levent -lutil
DPADD+= ${LIBEVENT} ${LIBUTIL}

.include <bsd.prog.mk>
