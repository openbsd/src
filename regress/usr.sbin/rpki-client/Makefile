# $OpenBSD: Makefile,v 1.12 2022/10/14 17:09:43 tb Exp $

SUBDIR += libressl
.if exists(/usr/local/bin/eopenssl11)
SUBDIR += openssl11
.else
.END:
	@echo 'Run "pkg_add openssl--%1.1" to run tests against OpenSSL 1.1'
	@echo SKIPPED
.endif

.include <bsd.subdir.mk>
.include <bsd.regress.mk>
