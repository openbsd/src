#	$OpenBSD: Makefile,v 1.6 2003/12/22 15:22:13 henning Exp $

PROG=	bgpd
SRCS=	bgpd.c buffer.c session.c log.c parse.y config.c imsg.c \
	rde.c rde_rib.c rde_decide.c rde_prefix.c mrt.c kroute.c
CFLAGS+= -Wall
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CLFAGS+= -Wmissing-declarations -Wredundant-decls
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
YFLAGS=
MAN= bgpd.8

CFLAGS+=	-Wall

.include <bsd.prog.mk>
