#	$OpenBSD: Makefile,v 1.1 2014/07/12 16:01:28 jsing Exp $

GO_VERSION != sh -c "(go version) 2>/dev/null || true"

.if empty(GO_VERSION)
regress:
	@echo golang is required for this regress... skipping
.endif

REGRESS_TARGETS=regress-goressl

regress-goressl:
	cd ${.CURDIR} && go test -test.v .

.include <bsd.regress.mk>
