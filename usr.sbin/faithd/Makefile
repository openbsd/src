#	$OpenBSD: Makefile,v 1.4 2002/09/08 01:20:15 itojun Exp $

PROG=	faithd
SRCS=	faithd.c tcp.c ftp.c prefix.c

MAN=	faithd.8

#CPPFLAGS+= -DFAITH4
CFLAGS+=-ansi -pedantic

LDADD+=	-lutil
DPADD+=	${LIBUTIL}

.include <bsd.prog.mk>
