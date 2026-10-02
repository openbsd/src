# $OpenBSD: Makefile,v 1.10 2026/10/02 16:48:01 schwarze Exp $

REGRESS_TARGETS  = noTH noarg onearg twoargs sixargs case longtit longvol
REGRESS_TARGETS += secsuffix
REGRESS_TARGETS += emptydate onlyyear isodate mdocdate baddate longdate
REGRESS_TARGETS += nobody

LINT_TARGETS	 = noTH noarg onearg twoargs sixargs case
LINT_TARGETS	+= emptydate baddate longdate nobody

SKIP_GROFF	 = noTH longtit longvol secsuffix longdate
SKIP_ASCII	 = noTH

.include <bsd.regress.mk>
