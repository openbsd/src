#	$OpenBSD: Makefile,v 1.1 2015/10/02 04:31:52 renato Exp $

.PATH:		${.CURDIR}/../eigrpd

PROG=	eigrpctl
SRCS=	util.c log.c eigrpctl.c parser.c
CFLAGS+= -Wall
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
CFLAGS+= -I${.CURDIR} -I${.CURDIR}/../eigrpd
LDADD=	-lutil
DPADD=	${LIBUTIL}
MAN=	eigrpctl.8

.include <bsd.prog.mk>
