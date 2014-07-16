#	$OpenBSD: Makefile,v 1.6 2014/07/16 16:56:45 okan Exp $

PROG=	caesar
MAN=	caesar.6
DPADD=	${LIBM}
LDADD=	-lm
MLINKS=	caesar.6 rot13.6
LINKS=	${BINDIR}/caesar ${BINDIR}/rot13

.include <bsd.prog.mk>
