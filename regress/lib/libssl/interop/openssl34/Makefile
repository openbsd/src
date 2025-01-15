# $OpenBSD: Makefile,v 1.1 2025/01/15 10:54:17 tb Exp $

.if ! exists(/usr/local/bin/eopenssl34)
regress:
	# install openssl-3.4 from ports for interop tests
	@echo 'Run "pkg_add openssl--%3.4" to run tests against OpenSSL 3.4'
	@echo SKIPPED
.else

PROGS =			client server
CFLAGS +=		-DOPENSSL_SUPPRESS_DEPRECATED
CPPFLAGS =		-I /usr/local/include/eopenssl34
LDFLAGS =		-L /usr/local/lib/eopenssl34
LDADD =			-lssl -lcrypto
DPADD =			/usr/local/lib/eopenssl34/libssl.a \
			/usr/local/lib/eopenssl34/libcrypto.a
LD_LIBRARY_PATH =	/usr/local/lib/eopenssl34
REGRESS_TARGETS =	run-self-client-server
.for p in ${PROGS}
REGRESS_TARGETS +=	run-ldd-$p run-version-$p run-protocol-$p
.endfor

.for p in ${PROGS}

run-ldd-$p: ldd-$p.out
	# check that $p is linked with OpenSSL 3.4
	grep -q /usr/local/lib/eopenssl34/libcrypto.so ldd-$p.out
	grep -q /usr/local/lib/eopenssl34/libssl.so ldd-$p.out
	# check that $p is not linked with LibreSSL
	! grep -v libc.so ldd-$p.out | grep /usr/lib/

run-version-$p: $p-self.out
	# check that runtime version is OpenSSL 3.4
	grep 'SSLEAY_VERSION: OpenSSL 3.4' $p-self.out

run-protocol-$p: $p-self.out
	# check that OpenSSL 3.4 protocol version is TLS 1.3
	grep 'Protocol *: TLSv1.3' $p-self.out

.endfor

.endif # exists(/usr/local/bin/eopenssl34)

.include <bsd.regress.mk>
