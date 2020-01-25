# $OpenBSD: Makefile,v 1.8 2020/01/25 16:09:05 jsing Exp $

SUBDIR =	libressl openssl openssl11

# the above binaries must have been built before we can continue
SUBDIR +=	cert
#SUBDIR +=	cipher
SUBDIR +=	netcat
SUBDIR +=	session

.include <bsd.subdir.mk>
