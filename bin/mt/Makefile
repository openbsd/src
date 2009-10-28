#	$OpenBSD: Makefile,v 1.8 2009/10/28 05:17:19 deraadt Exp $

COPTS+=	-DRMT
PROG=	mt
SRCS=	mt.c mtrmt.c
DPADD=	${LIBUTIL}
LDADD=	-lutil

LINKS=	${BINDIR}/mt ${BINDIR}/eject
MLINKS=	mt.1 eject.1

.include <bsd.prog.mk>
