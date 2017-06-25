# $OpenBSD: Makefile,v 1.1 2017/06/25 14:09:14 schwarze Exp $

REGRESS_TARGETS	 = args noME
LINT_TARGETS	 = args noME

# groff-1.22.3 defects:
# - Without .ME, .MT does not print the address.
# - But .ME prints <> even without .MT.

SKIP_GROFF	 = noME

.include <bsd.regress.mk>
