# $OpenBSD: Makefile,v 1.6 2017/03/08 22:53:36 schwarze Exp $

REGRESS_TARGETS	 = badargs call
LINT_TARGETS	 = badargs call

# groff-1.22.3 defect:
# - If the first argument of .St is the name of another macro,
#   internal groff_mdoc(7) state gets corrupted.

SKIP_GROFF	 = call

.include <bsd.regress.mk>
