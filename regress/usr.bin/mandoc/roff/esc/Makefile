# $OpenBSD: Makefile,v 1.21 2022/05/20 13:06:27 schwarze Exp $

REGRESS_TARGETS	 = one two multi comment
REGRESS_TARGETS	+= B bs_man bs_mdoc c c_man E1 e f h hneg l O1 o p r w z
REGRESS_TARGETS	+= ignore invalid unsupp
HTML_TARGETS	 = f
LINT_TARGETS	 = comment B h l O1 r w ignore invalid unsupp

# mandoc defects:
# - \h with a negative argument replaces output characters
#   instead of overstriking them
# - \r does not return to the previous output line

SKIP_GROFF	 = hneg r

.include <bsd.regress.mk>
