#	$OpenBSD: Makefile,v 1.4 2006/01/03 22:20:59 claudio Exp $

.PATH:		${.CURDIR}/../bgpd

PROG=	bgpctl
SRCS=	bgpctl.c parser.c buffer.c imsg.c util.c
CFLAGS+= -Wall
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CLFAGS+= -Wmissing-declarations -Wredundant-decls
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
CFLAGS+= -I${.CURDIR} -I${.CURDIR}/../bgpd
MAN=	bgpctl.8

.include <bsd.prog.mk>
