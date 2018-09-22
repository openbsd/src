# $OpenBSD: Makefile,v 1.2 2018/09/22 00:10:18 tb Exp $

GO_VERSION != sh -c "(go version) 2>/dev/null || true"

.if empty(GO_VERSION)
regress:
	@echo package go is required for this regress
	@echo SKIPPED
.endif

CLEANFILES+=wycheproof
REGRESS_TARGETS=regress-wycheproof

audit: wycheproof
	./wycheproof -v

wycheproof: wycheproof.go
	go build -o wycheproof ${.CURDIR}/wycheproof.go

regress-wycheproof: wycheproof
	./wycheproof

.PHONY: audit

.include <bsd.regress.mk>
