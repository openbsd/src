#	$OpenBSD: Makefile,v 1.3 2007/12/07 17:17:01 reyk Exp $

.PATH:		${.CURDIR}/../relayd

PROG=		relayctl
SRCS=		buffer.c imsg.c log.c relayctl.c parser.c

MAN=		relayctl.8

CFLAGS+=	-Wall -Werror -I${.CURDIR} -I${.CURDIR}/../relayd
CFLAGS+=	-Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare -Wbounded

.include <bsd.prog.mk>
