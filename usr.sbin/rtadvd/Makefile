#	$OpenBSD: Makefile,v 1.3 2001/12/01 23:27:23 miod Exp $

PROG=	rtadvd
SRCS=	rtadvd.c rrenum.c advcap.c if.c config.c timer.c dump.c

CPPFLAGS+=-DINET6
LDADD+=	-lcompat -lutil
DPADD+=	${LIBCOMPAT} ${LIBUTIL}
MAN=	rtadvd.8 rtadvd.conf.5

.include <bsd.prog.mk>
