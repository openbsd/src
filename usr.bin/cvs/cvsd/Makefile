# $Id: Makefile,v 1.2 2004/07/25 03:29:36 jfb Exp $

.PATH:	${.CURDIR}/..

PROG=cvsd
SRCS= cvsd.c conf.y log.c msg.c sock.c

BINDIR=/usr/sbin
MAN=cvsd.8 cvsd.conf.5

CFLAGS+= -Wall
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wsign-compare

.include <bsd.prog.mk>
