#	$OpenBSD: Makefile,v 1.2 2003/12/17 12:34:06 henning Exp $

PROG=	bgpd
SRCS=	bgpd.c buffer.c session.c log.c parse.y config.c imsg.c \
	rde.c rde_rib.c rde_decide.c rde_prefix.c mrt.c
CFLAGS+= -Wall -Wmissing-prototypes -Wno-uninitialized
CFLAGS+= -Wstrict-prototypes
CFLAGS+= -Wreturn-type -Wcast-qual -Wswitch
CFLAGS+= -Wpointer-arith -Wshadow
CFLAGS+= -Werror
YFLAGS=
NOMAN=

CFLAGS+=	-Wall

.include <bsd.prog.mk>
