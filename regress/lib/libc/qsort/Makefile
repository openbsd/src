#       $OpenBSD: Makefile,v 1.2 2017/05/17 20:28:35 millert Exp $

PROG=	qsort_test
CFLAGS+=-Wall

qsort-regress: ${PROG}
	./${PROG}

REGRESS_TARGETS=qsort-regress
.PHONY: ${REGRESS_TARGETS}

.include <bsd.regress.mk>
