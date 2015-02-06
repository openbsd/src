# $OpenBSD: Makefile,v 1.3 2015/02/06 11:54:03 schwarze Exp $

REGRESS_TARGETS	 = args noUE
LINT_TARGETS	 = args noUE

# groff-1.22.3 defects:
# - Without .UE, .UR does not print the URI.
# - But .UE prints <> even without .UR.

SKIP_GROFF	 = noUE

.include <bsd.regress.mk>
