# $OpenBSD: Makefile,v 1.12 2020/09/15 01:45:16 bluhm Exp $

SUBDIR =	libressl openssl openssl11

# the above binaries must have been built before we can continue
SUBDIR +=	cert
SUBDIR +=	cipher
SUBDIR +=	version
SUBDIR +=	netcat
SUBDIR +=	session
SUBDIR +=	botan

.include <bsd.subdir.mk>
