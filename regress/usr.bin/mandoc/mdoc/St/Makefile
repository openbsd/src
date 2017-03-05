# $OpenBSD: Makefile,v 1.5 2017/03/05 19:59:26 schwarze Exp $

REGRESS_TARGETS	 = badargs call
LINT_TARGETS	 = badargs call
MARKDOWN_TARGETS = badargs call

# groff-1.22.3 defect:
# - If the first argument of .St is the name of another macro,
#   internal groff_mdoc(7) state gets corrupted.

SKIP_GROFF	 = call

.include <bsd.regress.mk>
