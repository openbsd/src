#	$OpenBSD: Makefile,v 1.2 2017/08/15 23:46:51 schwarze Exp $

PROG =		uselocale
CFLAGS +=	-Wno-macro-redefined
LDFLAGS +=	-pthread

run-regress-${PROG}: ${PROG}
	./${PROG}

.include <bsd.regress.mk>
