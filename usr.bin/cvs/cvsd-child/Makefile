# $Id: Makefile,v 1.1 2005/02/22 22:33:01 jfb Exp $

.PATH:	${.CURDIR}/..

PROG=cvsd-child
SRCS= child.c buf.c log.c msg.c rcs.c rcsnum.c

BINDIR=/usr/sbin
MAN=

CFLAGS+= -Wall -I${.CURDIR}/..
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
CFLAGS+= -DCVSD
YFLAGS=

.include <bsd.prog.mk>
