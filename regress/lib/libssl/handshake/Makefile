#	$OpenBSD: Makefile,v 1.1 2019/01/20 06:47:38 tb Exp $

PROGS += valid_handshakes_terminate

.for p in ${PROGS}
REGRESS_TARGETS += run-$p
.endfor

LDADD =		${SSL_INT} -lcrypto
DPADD =		${LIBCRYPTO} ${LIBSSL}
WARNINGS =	Yes
CFLAGS +=	-DLIBRESSL_INTERNAL -Wundef -Werror -I$(BSDSRCDIR)/lib/libssl

.for p in ${PROGS}
run-$p: $p
	@echo '\n======== $@ ========'
	./$p
.endfor

.include <bsd.regress.mk>
