# $Id: Makefile,v 1.8 2004/12/08 18:52:53 xsa Exp $

.PATH:	${.CURDIR}/..

PROG=cvsd
SRCS= cvsd.c buf.c conf.y log.c msg.c rcs.c rcsnum.c session.c sock.c

BINDIR=/usr/sbin
MAN=cvsd.8 cvsd.conf.5

CFLAGS+= -Wall -I${.CURDIR}/..
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
CFLAGS+= -DCVSD
YFLAGS=

.include <bsd.prog.mk>
