#	$OpenBSD: Makefile,v 1.6 2002/10/26 20:16:13 itojun Exp $

PROG=	route6d
MAN=	route6d.8
LDADD+=	-lutil
DPADD+=	${LIBUTIL}

CPPFLAGS+=	-DINET6 -DHAVE_POLL_H

.include <bsd.prog.mk>
