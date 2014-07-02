# $OpenBSD: Makefile,v 1.6 2014/07/02 06:35:26 schwarze Exp $

REGRESS_TARGETS  = noTH noarg onearg twoargs sixargs case
REGRESS_TARGETS += emptydate onlyyear isodate mdocdate baddate
REGRESS_TARGETS += nobody

LINT_TARGETS = noTH case twoargs emptydate baddate nobody

SKIP_GROFF = noTH
SKIP_ASCII = noTH

.include <bsd.regress.mk>
