#	$OpenBSD: Makefile,v 1.21 2005/03/28 15:03:33 henning Exp $

.PATH:		${.CURDIR}/..

PROG=	bgpd
SRCS=	bgpd.c buffer.c session.c log.c parse.y config.c imsg.c \
	rde.c rde_rib.c rde_decide.c rde_prefix.c mrt.c kroute.c \
	control.c pfkey.c rde_update.c rde_attr.c printconf.c \
	rde_filter.c pftable.c leak.c
CFLAGS+= -Wall -I${.CURDIR}
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
YFLAGS=
MAN= bgpd.8 bgpd.conf.5

# kame scopeid hack
CPPFLAGS+=-DKAME_SCOPEID

.include <bsd.prog.mk>
