#	$OpenBSD: Makefile,v 1.12 2016/12/28 17:48:02 deraadt Exp $

CFLAGS+=-DSMALL -DNOSSL

PROG=	ftp
SRCS=	fetch.c ftp.c main.c small.c util.c
.PATH:	${.CURDIR}/../../../usr.bin/ftp

LDADD+=	-lutil
DPADD+=	${LIBUTIL}

.include <bsd.prog.mk>
