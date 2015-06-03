#	$OpenBSD: Makefile,v 1.7 2015/06/03 20:43:21 reyk Exp $

.PATH:		${.CURDIR}/../relayd

PROG=		relayctl
SRCS=		log.c relayctl.c parser.c

MAN=		relayctl.8

LDADD=		-lutil
DPADD=		${LIBUTIL}
CFLAGS+=	-Wall -I${.CURDIR} -I${.CURDIR}/../relayd
CFLAGS+=	-Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare
CDIAGFLAGS=

.include <bsd.prog.mk>
