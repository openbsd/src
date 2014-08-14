# $OpenBSD: Makefile,v 1.7 2014/08/14 02:00:52 schwarze Exp $

REGRESS_TARGETS  = noTH noarg onearg twoargs sixargs case
REGRESS_TARGETS += emptydate onlyyear isodate mdocdate baddate
REGRESS_TARGETS += nobody

LINT_TARGETS	 = noTH noarg onearg twoargs case
LINT_TARGETS	+= emptydate baddate nobody

SKIP_GROFF = noTH
SKIP_ASCII = noTH

.include <bsd.regress.mk>
