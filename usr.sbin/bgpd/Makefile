#	$OpenBSD: Makefile,v 1.16 2004/02/08 23:44:57 henning Exp $

.PATH:		${.CURDIR}/..

PROG=	bgpd
SRCS=	bgpd.c buffer.c session.c log.c parse.y config.c imsg.c \
	rde.c rde_rib.c rde_decide.c rde_prefix.c mrt.c kroute.c \
	control.c pfkey.c rde_update.c rde_attr.c printconf.c
CFLAGS+= -Wall -I${.CURDIR}
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CLFAGS+= -Wmissing-declarations -Wredundant-decls
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
YFLAGS=
MAN= bgpd.8 bgpd.conf.5

.include <bsd.prog.mk>
