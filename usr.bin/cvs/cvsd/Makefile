# $Id: Makefile,v 1.6 2004/12/06 18:57:10 xsa Exp $

.PATH:	${.CURDIR}/..

PROG=cvsd
SRCS= cvsd.c buf.c conf.y log.c msg.c rcs.c rcsnum.c session.c sock.c

BINDIR=/usr/sbin
MAN=cvsd.8 cvsd.conf.5

CFLAGS+= -Wall -I${.CURDIR}/..
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wsign-compare
CFLAGS+= -DCVSD

.include <bsd.prog.mk>
