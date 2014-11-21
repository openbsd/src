# $OpenBSD: Makefile,v 1.8 2014/11/21 01:52:45 schwarze Exp $

REGRESS_TARGETS  = noTH noarg onearg twoargs sixargs case longtit longvol
REGRESS_TARGETS += emptydate onlyyear isodate mdocdate baddate longdate
REGRESS_TARGETS += nobody

LINT_TARGETS	 = noTH noarg onearg twoargs case
LINT_TARGETS	+= emptydate baddate longdate nobody

SKIP_GROFF	 = noTH longtit longvol longdate
SKIP_ASCII	 = noTH

.include <bsd.regress.mk>
