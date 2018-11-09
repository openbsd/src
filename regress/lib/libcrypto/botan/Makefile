# $OpenBSD: Makefile,v 1.1 2018/11/09 23:44:32 bluhm Exp $

.if ! exists(/usr/local/bin/botan-test)
regress:
	# install botan2 from ports for botan tests with libressl provider
	@echo SKIPPED
.endif

# The Botan library from ports an be configured to use OpenSSL or
# LibreSSL as crypto provider.  When we run their regression tests,
# we are actually testing our library.  This is far from perfect.  A
# lot of LibreSSL features have not been implemented as Botan provider.
# Even if provider openssl is specified, botan-test runs a lot of
# non-openssl tests.  This can be improved later.

regress:
	/usr/local/bin/botan-test --provider=openssl

.include <bsd.regress.mk>
