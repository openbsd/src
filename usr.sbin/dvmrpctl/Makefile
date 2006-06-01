#	$OpenBSD: Makefile,v 1.1 2006/06/01 14:21:28 norby Exp $

.PATH:		${.CURDIR}/../dvmrpd

PROG=	dvmrpctl
SRCS=	buffer.c imsg.c log.c dvmrpctl.c parser.c

CFLAGS+= -Wall
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CLFAGS+= -Wmissing-declarations -Wredundant-decls
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
CFLAGS+= -I${.CURDIR} -I${.CURDIR}/../dvmrpd

MAN=	dvmrpctl.8

.include <bsd.prog.mk>
