# $OpenBSD: Makefile,v 1.22 2022/05/30 22:50:40 schwarze Exp $

REGRESS_TARGETS	 = one two multi comment
REGRESS_TARGETS	+= B bs_man bs_mdoc c c_man E1 e f h hneg l O1 o p r V1 w z
REGRESS_TARGETS	+= ignore invalid unsupp
HTML_TARGETS	 = f
LINT_TARGETS	 = comment B h l O1 r V1 w ignore invalid unsupp

# mandoc defects:
# - \h with a negative argument replaces output characters
#   instead of overstriking them
# - \r does not return to the previous output line

SKIP_GROFF	 = hneg r

# intentional difference:
# - mandoc does not inspect the environment

SKIP_GROFF	+= V1

.include <bsd.regress.mk>
