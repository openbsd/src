#	$OpenBSD: Makefile,v 1.29 2010/05/26 16:44:32 nicm Exp $

PROG=	bgpd
SRCS=	bgpd.c session.c log.c parse.y config.c \
	rde.c rde_rib.c rde_decide.c rde_prefix.c mrt.c kroute.c \
	control.c pfkey.c rde_update.c rde_attr.c printconf.c \
	rde_filter.c pftable.c name2id.c util.c carp.c timer.c
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
