#	$OpenBSD: Makefile,v 1.2 2004/01/21 23:45:18 henning Exp $

.PATH:		${.CURDIR}/../bgpd/

PROG=	bgpctl
SRCS=	bgpctl.c parser.c buffer.c imsg.c log.c
CFLAGS+= -Wall
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CLFAGS+= -Wmissing-declarations -Wredundant-decls
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
CFLAGS+= -I${.CURDIR} -I${.CURDIR}/../bgpd
MAN=	bgpctl.8

.include <bsd.prog.mk>
