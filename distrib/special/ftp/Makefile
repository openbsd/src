#	$OpenBSD: Makefile,v 1.8 2009/10/28 07:36:49 deraadt Exp $
#	$NetBSD: Makefile,v 1.13 1997/08/18 10:20:13 lukem Exp $
#	from: @(#)Makefile	8.2 (Berkeley) 4/3/94

CFLAGS+=-DSMALL -DINET6

PROG=	ftp
SRCS=	fetch.c ftp.c main.c small.c util.c
.PATH:	${.CURDIR}/../../../usr.bin/ftp

LDADD+=	-lcurses -lutil
DPADD+=	${LIBCURSES} ${LIBUTIL}

.include <bsd.prog.mk>
