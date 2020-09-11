# $OpenBSD: Makefile,v 1.10 2020/09/11 22:48:00 bluhm Exp $

SUBDIR =	libressl openssl openssl11

# the above binaries must have been built before we can continue
SUBDIR +=	cert
SUBDIR +=	cipher
SUBDIR +=	netcat
SUBDIR +=	session

.include <bsd.subdir.mk>
