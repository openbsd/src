# $OpenBSD: Makefile,v 1.3 2018/11/07 20:46:28 bluhm Exp $

PROGS =			client server
CPPFLAGS =
LDFLAGS =
LDADD =			-lssl -lcrypto
DPADD =			${LIBSSL} ${LIBCRYPTO}
LD_LIBRARY_PATH =

run-protocol-self-client run-protocol-self-server \
run-protocol-nc-client run-protocol-nc-server:
	@echo '\n======== $@ ========'
	# LibreSSL does not support TLS 1.3 yet
	@echo DISABLED

.for p in ${PROGS}
run-ldd-$p: ldd-$p.out
	@echo '\n======== $@ ========'
	# check that $p is linked with LibreSSL
	grep -q /usr/lib/libcrypto.so ldd-$p.out
	grep -q /usr/lib/libssl.so ldd-$p.out
	# check that $p is not linked with OpenSSL
	! grep /usr/local/lib/ ldd-$p.out
.endfor

.for o in nc-client nc-server self-client self-server
run-version-$o: $o.out
	@echo '\n======== $@ ========'
	# check that runtime version is LibreSSL
	grep 'SSLEAY_VERSION: LibreSSL' $o.out

run-protocol-$o: $o.out
	@echo '\n======== $@ ========'
	# check that protocol version is TLS 1.3
	grep 'Protocol *: TLSv1.3' $o.out
.endfor

.include <bsd.regress.mk>
