#	$OpenBSD: Makefile,v 1.4 2010/05/26 16:44:32 nicm Exp $

.PATH:		${.CURDIR}/../dvmrpd

PROG=	dvmrpctl
SRCS=	log.c dvmrpctl.c parser.c

CFLAGS+= -Wall
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
CFLAGS+= -I${.CURDIR} -I${.CURDIR}/../dvmrpd

LDADD=	-lutil
DPADD=	${LIBUTIL}

MAN=	dvmrpctl.8

.include <bsd.prog.mk>
