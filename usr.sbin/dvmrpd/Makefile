#	$OpenBSD: Makefile,v 1.1 2006/06/01 14:12:20 norby Exp $

PROG=	dvmrpd
SRCS=	ask_nbrs2.c buffer.c control.c dvmrpd.c dvmrpe.c graft.c graft_ack.c \
	group.c igmp.c imsg.c interface.c in_cksum.c kmroute.c kroute.c log.c \
	nbrs2.c neighbor.c packet.c parse.y printconf.c probe.c prune.c rde.c \
	rde_mfc.c rde_srt.c report.c

MAN=	dvmrpd.8 dvmrpd.conf.5

CFLAGS+= -Wall -I${.CURDIR}
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare -g
YFLAGS=
LDADD+=	-levent

.include <bsd.prog.mk>
