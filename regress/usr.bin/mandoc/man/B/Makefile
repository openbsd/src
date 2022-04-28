# $OpenBSD: Makefile,v 1.3 2022/04/28 10:17:38 schwarze Exp $

REGRESS_TARGETS	= args blank nest
LINT_TARGETS	= args blank nest

# groff-1.22.3 defect:
# - A blank line in next line scope causes a blank line.
SKIP_GROFF	= blank

.include <bsd.regress.mk>
