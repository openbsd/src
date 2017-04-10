#	$OpenBSD: Makefile,v 1.1 2017/04/10 13:35:42 florian Exp $

PROG=	slaacctl
SRCS=	slaacctl.c parser.c

MAN=	slaacctl.8

CFLAGS+= -Wall
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
CFLAGS+= -I${.CURDIR} -I${.CURDIR}/../slaacd
LDADD=	-lutil
DPADD=	${LIBUTIL}

.include <bsd.prog.mk>
