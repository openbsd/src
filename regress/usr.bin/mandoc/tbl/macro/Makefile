# $OpenBSD: Makefile,v 1.1.1.1 2015/01/29 23:24:24 schwarze Exp $

REGRESS_TARGETS	 = man nested
LINT_TARGETS	 = man nested

# trivial difference to groff-1.22.3:
# .TS in a table causes a blank table line in GNU tbl(1), but not in mandoc.

SKIP_GROFF	 = nested

.include <bsd.regress.mk>
