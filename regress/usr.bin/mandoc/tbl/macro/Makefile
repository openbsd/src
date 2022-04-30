# $OpenBSD: Makefile,v 1.3 2022/04/30 18:46:16 schwarze Exp $

REGRESS_TARGETS	 = column man nested req
LINT_TARGETS	 = man nested req

# mandoc defect:
# mandoc(1) ignores .br inside tbl(7) code.

# trivial differences to groff-1.23.0:
# .TS in a table causes a blank table line in GNU tbl(1), but not in mandoc.
# .TS in a column list causes a blank line in mandoc, but not in GNU tbl(1).

SKIP_GROFF	 = column nested req

.include <bsd.regress.mk>
