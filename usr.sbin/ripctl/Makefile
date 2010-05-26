#	$OpenBSD: Makefile,v 1.2 2010/05/26 16:44:32 nicm Exp $

.PATH:		${.CURDIR}/../ripd

PROG=	ripctl
SRCS=	log.c ripctl.c parser.c
CFLAGS+= -Wall
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
CFLAGS+= -I${.CURDIR} -I${.CURDIR}/../ripd
LDADD=	-lutil
DPADD=	${LIBUTIL}
MAN=	ripctl.8

.include <bsd.prog.mk>
