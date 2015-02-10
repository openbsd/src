# $OpenBSD: Makefile,v 1.4 2015/02/10 17:47:19 schwarze Exp $

REGRESS_TARGETS	 = badargs call
LINT_TARGETS	 = badargs call

# groff-1.22.3 defect:
# - If the first argument of .St is the name of another macro,
#   internal groff_mdoc(7) state gets corrupted.

SKIP_GROFF	 = call

.include <bsd.regress.mk>
