#	$OpenBSD: Makefile,v 1.5 2012/09/12 09:19:54 haesbaert Exp $

.PATH:		${.CURDIR}/../relayd

PROG=		relayctl
SRCS=		log.c relayctl.c parser.c

MAN=		relayctl.8

LDADD=		-lutil
DPADD=		${LIBUTIL}
CFLAGS+=	-Wall -Werror -I${.CURDIR} -I${.CURDIR}/../relayd
CFLAGS+=	-Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare -Wbounded
CDIAGFLAGS=

.include <bsd.prog.mk>
