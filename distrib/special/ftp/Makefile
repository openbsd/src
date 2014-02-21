#	$OpenBSD: Makefile,v 1.9 2014/02/21 18:30:15 deraadt Exp $

CFLAGS+=-DSMALL -DINET6

PROG=	ftp
SRCS=	fetch.c ftp.c main.c small.c util.c
.PATH:	${.CURDIR}/../../../usr.bin/ftp

LDADD+=	-lcurses -lutil
DPADD+=	${LIBCURSES} ${LIBUTIL}

.include <bsd.prog.mk>
