#	$OpenBSD: Makefile,v 1.4 2003/05/09 00:07:09 millert Exp $

PROG=	rtadvd
SRCS=	rtadvd.c rrenum.c advcap.c if.c config.c timer.c dump.c

CPPFLAGS+=-DINET6
LDADD+=	-lutil
DPADD+=	${LIBUTIL}
MAN=	rtadvd.8 rtadvd.conf.5

.include <bsd.prog.mk>
