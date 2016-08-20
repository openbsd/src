# $OpenBSD: Makefile,v 1.2 2016/08/20 14:43:40 schwarze Exp $

REGRESS_TARGETS	 = man nested column
LINT_TARGETS	 = man nested

# trivial differences to groff-1.22.3:
# .TS in a table causes a blank table line in GNU tbl(1), but not in mandoc.
# .TS in a column list causes a blank line in mandoc, but not in GNU tbl(1).

SKIP_GROFF	 = nested column

.include <bsd.regress.mk>
