#	$OpenBSD: Makefile,v 1.3 2002/05/09 14:12:16 itojun Exp $

PROG=	faithd
SRCS=	faithd.c tcp.c ftp.c prefix.c

MAN=	faithd.8

#CPPFLAGS+= -DFAITH4

LDADD+=	-lutil
DPADD+=	${LIBUTIL}

.include <bsd.prog.mk>
