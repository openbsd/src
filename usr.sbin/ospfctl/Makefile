#	$OpenBSD: Makefile,v 1.4 2010/05/26 16:44:32 nicm Exp $

.PATH:		${.CURDIR}/../ospfd

PROG=	ospfctl
SRCS=	log.c ospfctl.c parser.c
CFLAGS+= -Wall
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
CFLAGS+= -I${.CURDIR} -I${.CURDIR}/../ospfd
LDADD=	-lutil
DPADD=	${LIBUTIL}
MAN=	ospfctl.8

.include <bsd.prog.mk>
