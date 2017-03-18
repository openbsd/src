#	$OpenBSD: Makefile,v 1.1 2017/03/18 17:33:13 florian Exp $

PROG=	slaacdctl
SRCS=	slaacdctl.c parser.c

MAN=	slaacdctl.8

CFLAGS+= -Wall
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
CFLAGS+= -I${.CURDIR} -I${.CURDIR}/../slaacd
LDADD=	-lutil
DPADD=	${LIBUTIL}

.include <bsd.prog.mk>
