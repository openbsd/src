#	$OpenBSD: Makefile,v 1.6 2021/12/04 05:15:09 jsing Exp $

PROGS +=		ectest
PROGS +=		ec_asn1_test
PROGS +=		ec_point_conversion

.for t in ${PROGS}
REGRESS_TARGETS +=	run-$t
.endfor

LDADD =			${CRYPTO_INT}
DPADD =			${LIBCRYPTO}
WARNINGS =		Yes
CFLAGS +=		-DLIBRESSL_CRYPTO_INTERNAL -DLIBRESSL_INTERNAL
CFLAGS +=		-Wall -Wundef -Werror

CLEANFILES +=		${PROGS}

.for t in ${PROGS}
run-$t: $t
	./$t
.endfor

.include <bsd.regress.mk>
