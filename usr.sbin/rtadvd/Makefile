#	$OpenBSD: Makefile,v 1.1 1999/12/11 10:33:27 itojun Exp $

PROG=	rtadvd
SRCS=	rtadvd.c rrenum.c advcap.c if.c config.c timer.c

CPPFLAGS+=-DINET6
LDADD+=	-lcompat
DPADD+=	${LIBCOMPAT}
MAN=	rtadvd.8 rtadvd.conf.5

.include <bsd.prog.mk>
