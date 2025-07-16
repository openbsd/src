# $OpenBSD: Makefile,v 1.16 2025/07/16 15:21:45 tb Exp $

SUBDIR += libressl
.if exists(/usr/local/bin/eopenssl33) || exists(/usr/local/bin/eopenssl35)
SUBDIR += openssl
.endif

.include <bsd.subdir.mk>
.include <bsd.regress.mk>
