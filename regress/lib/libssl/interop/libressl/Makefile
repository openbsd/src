# $OpenBSD: Makefile,v 1.1.1.1 2018/11/07 01:08:49 bluhm Exp $

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
.endfor

.include <bsd.regress.mk>
