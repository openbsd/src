# $OpenBSD: Makefile,v 1.4 2025/07/24 17:10:22 schwarze Exp $

REGRESS_TARGETS	 = column man nested req
LINT_TARGETS	 = man nested req

# mandoc defect:
# mandoc(1) ignores .br inside tbl(7) code.

SKIP_GROFF	 = req

# trivial differences to groff-1.23.0:
# .TS in a column list causes a blank line in mandoc, but not in GNU tbl(1).

SKIP_GROFF	+= column

.include <bsd.regress.mk>
