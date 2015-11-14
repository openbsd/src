#	$OpenBSD: Makefile,v 1.10 2015/11/14 21:49:16 deraadt Exp $

CFLAGS+=-DSMALL -DINET6

PROG=	ftp
SRCS=	fetch.c ftp.c main.c small.c util.c
.PATH:	${.CURDIR}/../../../usr.bin/ftp

LDADD+=	-lutil
DPADD+=	${LIBUTIL}

.include <bsd.prog.mk>
