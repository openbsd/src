# $OpenBSD: Makefile,v 1.13 2024/09/04 04:35:05 tb Exp $

SUBDIR += libressl
.if exists(/usr/local/bin/eopenssl11)
SUBDIR += openssl11
.endif
.if exists(/usr/local/bin/eopenssl32)
SUBDIR += openssl
.endif

.include <bsd.subdir.mk>
.include <bsd.regress.mk>
