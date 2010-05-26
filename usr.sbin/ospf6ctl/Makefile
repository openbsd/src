#	$OpenBSD: Makefile,v 1.2 2010/05/26 16:44:32 nicm Exp $

.PATH:		${.CURDIR}/../ospf6d

PROG=	ospf6ctl
SRCS=	log.c ospf6ctl.c parser.c
CFLAGS+= -Wall
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
CFLAGS+= -I${.CURDIR} -I${.CURDIR}/../ospf6d
LDADD=	-lutil
DPADD=	${LIBUTIL}
MAN=	ospf6ctl.8

.include <bsd.prog.mk>
