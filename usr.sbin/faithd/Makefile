#	$OpenBSD: Makefile,v 1.5 2002/09/08 01:23:22 deraadt Exp $

PROG=	faithd
SRCS=	faithd.c tcp.c ftp.c prefix.c

MAN=	faithd.8

#CPPFLAGS+= -DFAITH4

LDADD+=	-lutil
DPADD+=	${LIBUTIL}

.include <bsd.prog.mk>
