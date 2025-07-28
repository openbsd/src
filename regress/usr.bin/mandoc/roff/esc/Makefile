# $OpenBSD: Makefile,v 1.24 2025/07/28 16:39:23 schwarze Exp $

REGRESS_TARGETS	 = one two multi comment
REGRESS_TARGETS	+= A1 B bs_man bs_mdoc c c_man E1 e f h hneg
REGRESS_TARGETS	+= l O1 o p r V1 w z
REGRESS_TARGETS	+= ignore invalid unsupp
HTML_TARGETS	 = f
LINT_TARGETS	 = comment A1 B h l O1 o r V1 w ignore invalid unsupp

# mandoc defects:
# - \h with a negative argument replaces output characters
#   instead of overstriking them
# - \r does not return to the previous output line

SKIP_GROFF	 = hneg r

# intentional difference:
# - mandoc does not inspect the environment

SKIP_GROFF	+= V1

# groff-1.23.0 regressions:
#  - \A and \w mis-parse an argument resulting from string expansion
#    or number register expansion; reported at
#    https://savannah.gnu.org/bugs/index.php?67372

SKIP_GROFF	+= A1 w

.include <bsd.regress.mk>
