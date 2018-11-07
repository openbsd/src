# $OpenBSD: Makefile,v 1.2 2018/11/07 06:29:26 bluhm Exp $

PROGS =			client server
CPPFLAGS =
LDFLAGS =
LDADD =			-lssl -lcrypto
DPADD =			${LIBSSL} ${LIBCRYPTO}
LD_LIBRARY_PATH =

.for p in ${PROGS}
run-ldd-$p: ldd-$p.out
	@echo '\n======== $@ ========'
	# check that $p is linked with LibreSSL
	grep -q /usr/lib/libcrypto.so ldd-$p.out
	grep -q /usr/lib/libssl.so ldd-$p.out
	# check that $p is not linked with OpenSSL
	! grep /usr/local/lib/ ldd-$p.out

run-version-$p: $p.out
	@echo '\n======== $@ ========'
	# check that runtime version is LibreSSL
	grep 'SSLEAY_VERSION: LibreSSL' $p.out
.endfor

.include <bsd.regress.mk>
