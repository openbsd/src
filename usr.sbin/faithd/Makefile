#	$OpenBSD: Makefile,v 1.1 1999/12/20 16:32:53 itojun Exp $

PROG=	faithd
SRCS=	faithd.c tcp.c ftp.c rsh.c

MAN=	faithd.8

#CPPFLAGS+= -DFAITH4

LDADD+=	-lutil
DPADD+=	${LIBUTIL}

.include <bsd.prog.mk>
