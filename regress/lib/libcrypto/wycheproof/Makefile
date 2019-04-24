# $OpenBSD: Makefile,v 1.3 2019/04/24 20:25:19 bluhm Exp $

.if ! (make(clean) || make(cleandir) || make(obj))
GO_VERSION != sh -c "(go version) 2>/dev/null || true"
.endif

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
