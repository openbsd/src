# $OpenBSD: Makefile,v 1.5 2018/11/11 00:15:04 bluhm Exp $

PROGS =			client server
CPPFLAGS =
LDFLAGS =
LDADD =			-lssl -lcrypto
DPADD =			${LIBSSL} ${LIBCRYPTO}
LD_LIBRARY_PATH =
REGRESS_TARGETS =	run-self-client-server
.for p in ${PROGS}
REGRESS_TARGETS +=	run-ldd-$p run-version-$p run-protocol-$p
.endfor

.for p in ${PROGS}

run-ldd-$p: ldd-$p.out
	@echo '\n======== $@ ========'
	# check that $p is linked with LibreSSL
	grep -q /usr/lib/libcrypto.so ldd-$p.out
	grep -q /usr/lib/libssl.so ldd-$p.out
	# check that $p is not linked with OpenSSL
	! grep /usr/local/lib/ ldd-$p.out

run-version-$p: $p-self.out
	@echo '\n======== $@ ========'
	# check that runtime version is LibreSSL
	grep 'SSLEAY_VERSION: LibreSSL' $p-self.out

run-protocol-$p: $p-self.out
	@echo '\n======== $@ ========'
	# check that LibreSSL protocol version is TLS 1.2
	# XXX adapt when LibreSSL supports TLS 1.3
	grep 'Protocol *: TLSv1.2' $p-self.out

.endfor

.include <bsd.regress.mk>
