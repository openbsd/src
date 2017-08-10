#	$OpenBSD: Makefile,v 1.1 2017/08/10 14:45:42 schwarze Exp $

PROG =		uselocale
LDFLAGS +=	-pthread

run-regress-${PROG}: ${PROG}
	./${PROG}

.include <bsd.regress.mk>
