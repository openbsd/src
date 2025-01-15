# $OpenBSD: Makefile,v 1.15 2025/01/15 11:00:09 tb Exp $

SUBDIR += libressl
.if exists(/usr/local/bin/eopenssl33)
SUBDIR += openssl
.endif

.include <bsd.subdir.mk>
.include <bsd.regress.mk>
