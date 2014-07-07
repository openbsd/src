# $OpenBSD: Makefile,v 1.2 2014/07/07 21:35:42 schwarze Exp $

REGRESS_TARGETS	= args noUE
LINT_TARGETS	= noUE

# groff-1.22.2 defect:
# - without .UE, .UR does not print the URI.

SKIP_GROFF	= noUE

.include <bsd.regress.mk>
