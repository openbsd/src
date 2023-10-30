# $OpenBSD: Makefile,v 1.18 2023/10/30 17:15:21 tb Exp $

SUBDIR =	libressl openssl11 openssl30 openssl31

# the above binaries must have been built before we can continue
SUBDIR +=	netcat
SUBDIR +=	session
SUBDIR +=	botan

# What is below takes a long time.
# setting REGRESS_SKIP_SLOW to "yes" in mk.conf
# will skip the tests that do not test libressl
# but do things like test openssl11 to openssl30
SUBDIR +=	version
SUBDIR +=	cipher
# This takes a really long time. 
SUBDIR +=	cert

.include <bsd.subdir.mk>
