#	$OpenBSD: Makefile,v 1.3 2003/12/20 15:10:33 henning Exp $

PROG=	bgpd
SRCS=	bgpd.c buffer.c session.c log.c parse.y config.c imsg.c \
	rde.c rde_rib.c rde_decide.c rde_prefix.c mrt.c
CFLAGS+= -Wall
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CLFAGS+= -Wmissing-declarations -Wredundant-decls
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual -Wcast-align
CFLAGS+= -Wsign-compare
CFLAGS+= -Werror
YFLAGS=
NOMAN=

CFLAGS+=	-Wall

.include <bsd.prog.mk>
