# $Id: Makefile,v 1.5 2004/11/25 18:30:12 jfb Exp $

.PATH:	${.CURDIR}/..

PROG=cvsd
SRCS= cvsd.c buf.c conf.y log.c msg.c rcs.c rcsnum.c session.c sock.c

BINDIR=/usr/sbin
MAN=cvsd.8 cvsd.conf.5

CFLAGS+= -Wall
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wsign-compare
CFLAGS+= -DCVSD

.include <bsd.prog.mk>
