#	$OpenBSD: Makefile,v 1.2 2000/05/23 11:23:22 itojun Exp $

PROG=	rtadvd
SRCS=	rtadvd.c rrenum.c advcap.c if.c config.c timer.c dump.c

CPPFLAGS+=-DINET6
LDADD+=	-lcompat
DPADD+=	${LIBCOMPAT}
MAN=	rtadvd.8 rtadvd.conf.5

.include <bsd.prog.mk>
