# $OpenBSD: Makefile,v 1.1 2023/10/30 17:15:21 tb Exp $

.if !exists(/usr/local/bin/eopenssl31)
regress:
	# install openssl-3.1 from ports for interop tests
	@echo 'Run "pkg_add openssl--%3.1" to run tests against OpenSSL 3.1'
	@echo SKIPPED
.else

PROGS =			client server
CPPFLAGS =		-I /usr/local/include/eopenssl31
LDFLAGS =		-L /usr/local/lib/eopenssl31
LDADD =			-lssl -lcrypto
DPADD =			/usr/local/lib/eopenssl31/libssl.a \
			/usr/local/lib/eopenssl31/libcrypto.a
LD_LIBRARY_PATH =	/usr/local/lib/eopenssl31
REGRESS_TARGETS =	run-self-client-server
.for p in ${PROGS}
REGRESS_TARGETS +=	run-ldd-$p run-version-$p run-protocol-$p
.endfor

.for p in ${PROGS}

run-ldd-$p: ldd-$p.out
	# check that $p is linked with OpenSSL 3.1
	grep -q /usr/local/lib/eopenssl31/libcrypto.so ldd-$p.out
	grep -q /usr/local/lib/eopenssl31/libssl.so ldd-$p.out
	# check that $p is not linked with LibreSSL
	! grep -v libc.so ldd-$p.out | grep /usr/lib/

run-version-$p: $p-self.out
	# check that runtime version is OpenSSL 3.1
	grep 'SSLEAY_VERSION: OpenSSL 3.1' $p-self.out

run-protocol-$p: $p-self.out
	# check that OpenSSL 3.1 protocol version is TLS 1.3
	grep 'Protocol *: TLSv1.3' $p-self.out

.endfor

.endif # exists(/usr/local/bin/eopenssl31)

.include <bsd.regress.mk>
