# $OpenBSD: Makefile,v 1.26 2026/08/08 14:21:31 schwarze Exp $

REGRESS_TARGETS	 = one two multi comment
REGRESS_TARGETS	+= A1 B bs_man bs_mdoc c c_man E1 e f h hneg
REGRESS_TARGETS	+= l O1 o p r V1 w z
REGRESS_TARGETS	+= argmiss ignore invalid unsupp
HTML_TARGETS	 = f
LINT_TARGETS	 = comment A1 B h l O1 o r V1 w argmiss ignore invalid unsupp

# mandoc defects:
# - \h with a negative argument replaces output characters
#   instead of overstriking them
# - \r does not return to the previous output line

SKIP_GROFF	 = hneg r

# intentional differences:
# - mandoc does not inspect the environment
# - mandoc does not disregard the newline after a missing argument

SKIP_GROFF	+= V1 argmiss

.include <bsd.regress.mk>
