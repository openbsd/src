#	$OpenBSD: Makefile,v 1.5 2001/12/01 23:27:23 miod Exp $

PROG=	route6d
MAN=	route6d.8
LDADD+=	-lutil
DPADD+=	${LIBUTIL}

CPPFLAGS+=	-DINET6

.include <bsd.prog.mk>
