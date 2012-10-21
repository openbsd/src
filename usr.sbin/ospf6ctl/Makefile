#	$OpenBSD: Makefile,v 1.3 2012/10/21 21:30:44 bluhm Exp $

.PATH:		${.CURDIR}/../ospf6d

PROG=	ospf6ctl
SRCS=	log.c ospf6ctl.c parser.c util.c
CFLAGS+= -Wall
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
CFLAGS+= -I${.CURDIR} -I${.CURDIR}/../ospf6d
LDADD=	-lutil
DPADD=	${LIBUTIL}
MAN=	ospf6ctl.8

.include <bsd.prog.mk>
