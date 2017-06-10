# $OpenBSD: Makefile,v 1.17 2017/06/10 01:48:31 schwarze Exp $

REGRESS_TARGETS	 = badNAME badNAMEuse break broken
REGRESS_TARGETS	+= empty emptyNAME emptyNAMEuse
REGRESS_TARGETS	+= font long par parns punct

LINT_TARGETS	 = badNAME badNAMEuse break broken punct

# groff-1.22.3/mandoc differences:
# - When the head of an Nm block in the SYNOPSIS is broken by an
#   explicit block end macro on the same line, formatting differs,
#   but doesn't make sense either way.
# - Groff doesn't support the nS register.

SKIP_GROFF	 = broken parns
SKIP_TMAN	 = broken par parns

# groff-1.22.3 defect:
# When a SYNOPSIS Nm block head breaks a sub block, all the
# remaining content in the document gets lost.

SKIP_GROFF	+= break

SKIP_MARKDOWN	?= badNAME badNAMEuse emptyNAME emptyNAMEuse

.include <bsd.regress.mk>
