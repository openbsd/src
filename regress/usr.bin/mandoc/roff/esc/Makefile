# $OpenBSD: Makefile,v 1.20 2022/05/01 16:18:59 schwarze Exp $

REGRESS_TARGETS	 = one two multi comment
REGRESS_TARGETS	+= B bs_man bs_mdoc c c_man E1 e f h hneg l O1 o p w z
REGRESS_TARGETS	+= ignore invalid unsupp
HTML_TARGETS	 = f
LINT_TARGETS	 = comment B h l O1 w ignore invalid unsupp

# mandoc defect:
# - \h with a negative argument replaces output characters
#   instead of overstriking them

SKIP_GROFF	 = hneg

.include <bsd.regress.mk>
