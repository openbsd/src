# $OpenBSD: Makefile,v 1.9 2015/02/06 11:54:03 schwarze Exp $

REGRESS_TARGETS  = noTH noarg onearg twoargs sixargs case longtit longvol
REGRESS_TARGETS += emptydate onlyyear isodate mdocdate baddate longdate
REGRESS_TARGETS += nobody

LINT_TARGETS	 = noTH noarg onearg twoargs sixargs case
LINT_TARGETS	+= emptydate baddate longdate nobody

SKIP_GROFF	 = noTH longtit longvol longdate
SKIP_ASCII	 = noTH

.include <bsd.regress.mk>
