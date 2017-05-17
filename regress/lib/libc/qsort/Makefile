#       $OpenBSD: Makefile,v 1.1 2017/05/17 14:47:06 millert Exp $

PROG=	qsort_test
CFLAGS+=-Wall
LDADD+=	-lm
DPADD+=	${LIBM}

qsort-regress: ${PROG}
	./${PROG}

REGRESS_TARGETS=qsort-regress
.PHONY: ${REGRESS_TARGETS}

.include <bsd.regress.mk>
