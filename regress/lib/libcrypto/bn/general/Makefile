#	$OpenBSD: Makefile,v 1.5 2019/04/13 22:06:31 tb Exp $

.include "../../Makefile.inc"

PROGS +=	bntest
PROGS +=	bn_to_string

LDADD =		${CRYPTO_INT}
DPADD =		${LIBCRYPTO}
WARNINGS =	Yes
CFLAGS +=	-Werror

.for p in ${PROGS}
REGRESS_TARGETS += run-$p

run-$p: $p
	./$p

.PHONY: run-$p
.endfor

.include <bsd.regress.mk>
