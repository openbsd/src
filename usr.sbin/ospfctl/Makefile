#	$OpenBSD: Makefile,v 1.5 2016/09/02 14:02:48 benno Exp $

.PATH:		${.CURDIR}/../ospfd

PROG=	ospfctl
SRCS=	logmsg.c ospfctl.c parser.c
CFLAGS+= -Wall
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
CFLAGS+= -I${.CURDIR} -I${.CURDIR}/../ospfd
LDADD=	-lutil
DPADD=	${LIBUTIL}
MAN=	ospfctl.8

.include <bsd.prog.mk>
