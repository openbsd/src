# $Id: Makefile,v 1.3 2004/07/29 15:33:05 jfb Exp $

.PATH:	${.CURDIR}/..

PROG=cvsd
SRCS= cvsd.c conf.y log.c msg.c sock.c

BINDIR=/usr/sbin
MAN=cvsd.8 cvsd.conf.5

CFLAGS+= -Wall
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wsign-compare
CFLAGS+= -DCVSD

.include <bsd.prog.mk>
