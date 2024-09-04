# $OpenBSD: Makefile,v 1.14 2024/09/04 04:35:30 tb Exp $

SUBDIR += libressl
.if exists(/usr/local/bin/eopenssl32)
SUBDIR += openssl
.endif

.include <bsd.subdir.mk>
.include <bsd.regress.mk>
