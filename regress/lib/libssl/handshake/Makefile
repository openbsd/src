#	$OpenBSD: Makefile,v 1.2 2019/01/23 04:50:24 tb Exp $

PROGS += handshake_table
PROGS += valid_handshakes_terminate

.for p in ${PROGS}
REGRESS_TARGETS += run-$p
.endfor

LDADD =		${SSL_INT} -lcrypto
DPADD =		${LIBCRYPTO} ${LIBSSL}
WARNINGS =	Yes
CFLAGS +=	-DLIBRESSL_INTERNAL -Wundef -Werror -I$(BSDSRCDIR)/lib/libssl

print:	handshake_table
	./handshake_table -C

.for p in ${PROGS}
run-$p: $p
	@echo '\n======== $@ ========'
	./$p
.endfor

.include <bsd.regress.mk>
