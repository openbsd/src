#	$OpenBSD: Makefile,v 1.2 2001/02/15 17:37:33 itojun Exp $

PROG=	faithd
SRCS=	faithd.c tcp.c ftp.c rsh.c prefix.c

MAN=	faithd.8

#CPPFLAGS+= -DFAITH4

LDADD+=	-lutil
DPADD+=	${LIBUTIL}

.include <bsd.prog.mk>
