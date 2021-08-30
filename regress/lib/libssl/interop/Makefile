# $OpenBSD: Makefile,v 1.15 2021/08/30 17:34:02 tb Exp $

SUBDIR =	libressl openssl openssl11

# the above binaries must have been built before we can continue
SUBDIR +=	netcat
SUBDIR +=	session
SUBDIR +=	botan

# What is below takes a long time.
# setting REGRESS_SKIP_SLOW to "yes" in mk.conf
# will skip the tests that do not test libressl
# but do things like test openssl to openssl11
SUBDIR +=	version
SUBDIR +=	cipher
# This takes a really long time. 
SUBDIR +=	cert

.include <bsd.subdir.mk>
