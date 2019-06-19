#	$OpenBSD: Makefile,v 1.14 2019/05/16 12:44:17 florian Exp $

CFLAGS+=-DSMALL -DNOSSL

PROG=	ftp
SRCS=	fetch.c ftp.c main.c small.c util.c
.PATH:	${.CURDIR}/../../../usr.bin/ftp

LDADD+=	-lutil
DPADD+=	${LIBUTIL}

.include <bsd.prog.mk>
