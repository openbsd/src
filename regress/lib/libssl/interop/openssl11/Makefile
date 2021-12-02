# $OpenBSD: Makefile,v 1.8 2021/12/02 17:10:53 kn Exp $

.if ! exists(/usr/local/bin/eopenssl11)
regress:
	# install openssl-1.1 from ports for interop tests
	@echo 'Run "pkg_add openssl--%1.1" to run tests against OpenSSL 1.1'
	@echo SKIPPED
.else

PROGS =			client server
CPPFLAGS =		-I /usr/local/include/eopenssl11
LDFLAGS =		-L /usr/local/lib/eopenssl11
LDADD =			-lssl -lcrypto
DPADD =			/usr/local/lib/eopenssl11/libssl.a \
			/usr/local/lib/eopenssl11/libcrypto.a
LD_LIBRARY_PATH =	/usr/local/lib/eopenssl11
REGRESS_TARGETS =	run-self-client-server
.for p in ${PROGS}
REGRESS_TARGETS +=	run-ldd-$p run-version-$p run-protocol-$p
.endfor

.for p in ${PROGS}

run-ldd-$p: ldd-$p.out
	# check that $p is linked with OpenSSL 1.1
	grep -q /usr/local/lib/eopenssl11/libcrypto.so ldd-$p.out
	grep -q /usr/local/lib/eopenssl11/libssl.so ldd-$p.out
	# check that $p is not linked with LibreSSL
	! grep -v libc.so ldd-$p.out | grep /usr/lib/

run-version-$p: $p-self.out
	# check that runtime version is OpenSSL 1.1
	grep 'SSLEAY_VERSION: OpenSSL 1.1' $p-self.out

run-protocol-$p: $p-self.out
	# check that OpenSSL 1.1 protocol version is TLS 1.3
	grep 'Protocol *: TLSv1.3' $p-self.out

.endfor

.endif # exists(/usr/local/bin/eopenssl11)

.include <bsd.regress.mk>
