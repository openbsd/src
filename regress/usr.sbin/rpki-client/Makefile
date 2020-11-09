# $OpenBSD: Makefile,v 1.9 2020/11/09 15:43:46 tb Exp $

SUBDIR += libressl
.if exists(/usr/local/bin/eopenssl11)
SUBDIR += openssl11
.endif

.include <bsd.subdir.mk>
.include <bsd.regress.mk>
