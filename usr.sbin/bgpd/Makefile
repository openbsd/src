#	$OpenBSD: Makefile,v 1.34 2018/09/07 10:49:22 claudio Exp $

PROG=	bgpd
SRCS=	bgpd.c session.c log.c logmsg.c parse.y config.c \
	rde.c rde_rib.c rde_decide.c rde_prefix.c mrt.c kroute.c \
	control.c pfkey.c rde_update.c rde_attr.c printconf.c \
	rde_filter.c rde_sets.c rde_trie.c pftable.c name2id.c \
	util.c carp.c timer.c
CFLAGS+= -Wall -I${.CURDIR}
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
YFLAGS=
LDADD+=	-lutil
DPADD+= ${LIBUTIL}
MAN= bgpd.8 bgpd.conf.5

.include <bsd.prog.mk>
