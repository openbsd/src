# $OpenBSD: Makefile,v 1.21 2025/01/15 10:54:17 tb Exp $

SUBDIR =	libressl openssl33 openssl34

# the above binaries must have been built before we can continue
SUBDIR +=	netcat
SUBDIR +=	session
SUBDIR +=	botan

# What is below takes a long time.
# setting REGRESS_SKIP_SLOW to "yes" in mk.conf
# will skip the tests that do not test libressl
# but do things like test openssl 3.x to openssl 3.y
SUBDIR +=	version
SUBDIR +=	cipher
# This takes a really long time. 
SUBDIR +=	cert

.include <bsd.subdir.mk>
