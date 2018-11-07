# $OpenBSD: Makefile,v 1.1.1.1 2018/11/07 01:08:49 bluhm Exp $

.if ! exists(/usr/local/bin/eopenssl)
regress:
	# install openssl-1.0.2p from ports for interop tests
	@echo SKIPPED
.endif

PROGS =			client server
CPPFLAGS =		-I /usr/local/include/eopenssl
LDFLAGS =		-L /usr/local/lib/eopenssl
LDADD =			-lssl -lcrypto
DPADD =			/usr/local/lib/eopenssl/libssl.a \
			/usr/local/lib/eopenssl/libcrypto.a
LD_LIBRARY_PATH =	/usr/local/lib/eopenssl

.for p in ${PROGS}
run-ldd-$p: ldd-$p.out
	@echo '\n======== $@ ========'
	# check that $p is linked with OpenSSL
	grep -q /usr/local/lib/eopenssl/libcrypto.so ldd-$p.out
	grep -q /usr/local/lib/eopenssl/libssl.so ldd-$p.out
	# check that $p is not linked with LibreSSL
	! grep -v libc.so ldd-$p.out | grep /usr/lib/
.endfor

.include <bsd.regress.mk>
