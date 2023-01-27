# $OpenBSD: Makefile,v 1.1 2023/01/27 08:28:36 tb Exp $

.if ! exists(/usr/local/bin/eopenssl30)
regress:
	# install openssl-3.0 from ports for interop tests
	@echo 'Run "pkg_add openssl--%3.0" to run tests against OpenSSL 3.0'
	@echo SKIPPED
.else

PROGS =			client server
CPPFLAGS =		-I /usr/local/include/eopenssl30
LDFLAGS =		-L /usr/local/lib/eopenssl30
LDADD =			-lssl -lcrypto
DPADD =			/usr/local/lib/eopenssl30/libssl.a \
			/usr/local/lib/eopenssl30/libcrypto.a
LD_LIBRARY_PATH =	/usr/local/lib/eopenssl30
REGRESS_TARGETS =	run-self-client-server
.for p in ${PROGS}
REGRESS_TARGETS +=	run-ldd-$p run-version-$p run-protocol-$p
.endfor

.for p in ${PROGS}

run-ldd-$p: ldd-$p.out
	# check that $p is linked with OpenSSL 3.0
	grep -q /usr/local/lib/eopenssl30/libcrypto.so ldd-$p.out
	grep -q /usr/local/lib/eopenssl30/libssl.so ldd-$p.out
	# check that $p is not linked with LibreSSL
	! grep -v libc.so ldd-$p.out | grep /usr/lib/

run-version-$p: $p-self.out
	# check that runtime version is OpenSSL 3.0
	grep 'SSLEAY_VERSION: OpenSSL 3.0' $p-self.out

run-protocol-$p: $p-self.out
	# check that OpenSSL 3.0 protocol version is TLS 1.3
	grep 'Protocol *: TLSv1.3' $p-self.out

.endfor

.endif # exists(/usr/local/bin/eopenssl30)

.include <bsd.regress.mk>
