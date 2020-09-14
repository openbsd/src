# $OpenBSD: Makefile,v 1.11 2020/09/14 00:51:04 bluhm Exp $

SUBDIR =	libressl openssl openssl11

# the above binaries must have been built before we can continue
SUBDIR +=	cert
SUBDIR +=	cipher
SUBDIR +=	version
SUBDIR +=	netcat
SUBDIR +=	session

.include <bsd.subdir.mk>
