# $OpenBSD: Makefile,v 1.1 2024/08/18 09:14:17 tb Exp $

.if ! exists(/usr/local/bin/eopenssl32)
regress:
	# install openssl-3.2 ports for interop tests
	@echo 'Run "pkg_add openssl--%3.2" to run tests against OpenSSL 3.2'
	@echo SKIPPED
.else

PROGS =			client server
CPPFLAGS =		-I /usr/local/include/eopenssl32
LDFLAGS =		-L /usr/local/lib/eopenssl32
LDADD =			-lssl -lcrypto
DPADD =			/usr/local/lib/eopenssl32/libssl.a \
			/usr/local/lib/eopenssl32/libcrypto.a
LD_LIBRARY_PATH =	/usr/local/lib/eopenssl32
REGRESS_TARGETS =	run-self-client-server
.for p in ${PROGS}
REGRESS_TARGETS +=	run-ldd-$p run-version-$p run-protocol-$p
.endfor

.for p in ${PROGS}

run-ldd-$p: ldd-$p.out
	# check that $p is linked with OpenSSL 3.2
	grep -q /usr/local/lib/eopenssl32/libcrypto.so ldd-$p.out
	grep -q /usr/local/lib/eopenssl32/libssl.so ldd-$p.out
	# check that $p is not linked with LibreSSL
	! grep -v libc.so ldd-$p.out | grep /usr/lib/

run-version-$p: $p-self.out
	# check that runtime version is OpenSSL 3.2
	grep 'SSLEAY_VERSION: OpenSSL 3.2' $p-self.out

run-protocol-$p: $p-self.out
	# check that OpenSSL 3.2 protocol version is TLS 1.3
	grep 'Protocol *: TLSv1.3' $p-self.out

.endfor

.endif # exists(/usr/local/bin/eopenssl32)

.include <bsd.regress.mk>
