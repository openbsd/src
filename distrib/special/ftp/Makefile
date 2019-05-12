#	$OpenBSD: Makefile,v 1.13 2019/05/12 20:44:39 kmos Exp $

CFLAGS+=-DSMALL -DNOSSL

PROG=	ftp
SRCS=	file.c ftp.c http.c main.c progressmeter.c url.c util.c xmalloc.c
.PATH:	${.CURDIR}/../../../usr.bin/ftp

LDADD+=	-lutil
DPADD+=	${LIBUTIL}

.include <bsd.prog.mk>
