#	$OpenBSD: Makefile,v 1.3 2006/08/17 16:48:02 claudio Exp $

.PATH:		${.CURDIR}/../dvmrpd

PROG=	dvmrpctl
SRCS=	buffer.c imsg.c log.c dvmrpctl.c parser.c

CFLAGS+= -Wall
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
CFLAGS+= -I${.CURDIR} -I${.CURDIR}/../dvmrpd

MAN=	dvmrpctl.8

.include <bsd.prog.mk>
