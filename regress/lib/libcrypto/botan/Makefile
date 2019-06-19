# $OpenBSD: Makefile,v 1.2 2019/05/14 05:48:58 bluhm Exp $

.if ! exists(/usr/local/bin/botan-test)
regress:
	# install botan2 from ports for botan tests with libressl provider
	# pkg_add botan2
	@echo SKIPPED
.endif

# The Botan library from ports an be configured to use OpenSSL or
# LibreSSL as crypto provider.  When we run their regression tests,
# we are actually testing our library.  This is far from perfect.  A
# lot of LibreSSL features have not been implemented as Botan provider.
# Even if provider openssl is specified, botan-test runs a lot of
# non-openssl tests.  This can be improved later.

REGRESS_TARGETS=	run-regress
run-regress:
	/usr/local/bin/botan-test --provider=openssl

.include <bsd.regress.mk>
