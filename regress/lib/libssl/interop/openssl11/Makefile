# $OpenBSD: Makefile,v 1.3 2018/11/09 06:30:41 bluhm Exp $

.if ! exists(/usr/local/bin/eopenssl11)
regress:
	# install openssl-1.1.1 from ports for interop tests
	@echo SKIPPED
.endif

PROGS =			client server
CPPFLAGS =		-I /usr/local/include/eopenssl11
LDFLAGS =		-L /usr/local/lib/eopenssl11
LDADD =			-lssl -lcrypto
DPADD =			/usr/local/lib/eopenssl11/libssl.a \
			/usr/local/lib/eopenssl11/libcrypto.a
LD_LIBRARY_PATH =	/usr/local/lib/eopenssl11
REGRESS_TARGETS =	run-self
.for p in ${PROGS}
REGRESS_TARGETS +=	run-ldd-$p run-$p
.for x in nc self
REGRESS_TARGETS +=	run-version-$x-$p run-protocol-$x-$p
.endfor
.endfor

run-protocol-nc-client run-protocol-nc-server:
	@echo '\n======== $@ ========'
	# LibreSSL does not support TLS 1.3 yet
	@echo DISABLED

.for p in ${PROGS}
run-ldd-$p: ldd-$p.out
	@echo '\n======== $@ ========'
	# check that $p is linked with OpenSSL 1.1
	grep -q /usr/local/lib/eopenssl11/libcrypto.so ldd-$p.out
	grep -q /usr/local/lib/eopenssl11/libssl.so ldd-$p.out
	# check that $p is not linked with LibreSSL
	! grep -v libc.so ldd-$p.out | grep /usr/lib/
.endfor

.for o in nc-client nc-server self-client self-server
run-version-$o: $o.out
	@echo '\n======== $@ ========'
	# check that runtime version is OpenSSL 1.1
	grep 'SSLEAY_VERSION: OpenSSL 1.1' $o.out

run-protocol-$o: $o.out
	@echo '\n======== $@ ========'
	# check that protocol version is TLS 1.3
	grep 'Protocol *: TLSv1.3' $o.out
.endfor

.include <bsd.regress.mk>
