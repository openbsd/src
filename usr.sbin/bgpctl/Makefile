#	$OpenBSD: Makefile,v 1.1 2004/01/02 02:22:52 henning Exp $

.PATH:		${.CURDIR}/../bgpd/

PROG=	bgpctl
SRCS=	bgpctl.c buffer.c imsg.c log.c
CFLAGS+= -Wall
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CLFAGS+= -Wmissing-declarations -Wredundant-decls
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
CFLAGS+= -I${.CURDIR}/../bgpd/
MAN=	bgpctl.8

.include <bsd.prog.mk>
