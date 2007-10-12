#	$OpenBSD: Makefile,v 1.1 2007/10/12 08:22:53 norby Exp $

.PATH:		${.CURDIR}/../ospf6d

PROG=	ospf6ctl
SRCS=	buffer.c imsg.c log.c ospf6ctl.c parser.c
CFLAGS+= -Wall
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
CFLAGS+= -I${.CURDIR} -I${.CURDIR}/../ospf6d
MAN=	ospf6ctl.8

.include <bsd.prog.mk>
