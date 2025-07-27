# $OpenBSD: Makefile,v 1.4 2025/07/27 16:14:04 schwarze Exp $

REGRESS_TARGETS	= args blank nest
LINT_TARGETS	= args blank nest

# groff-1.23.0 defect:
#  - A blank line in next line scope causes a blank line.

SKIP_GROFF	 = blank

# groff-1.23.0 regression:
#  - I to BI font remapping in section headers
#    fails to extend to next line scope.

SKIP_GROFF	+= nest

.include <bsd.regress.mk>
