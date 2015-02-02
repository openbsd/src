# $OpenBSD: Makefile,v 1.11 2015/02/02 04:04:18 schwarze Exp $

REGRESS_TARGETS	 = badNAME badNAMEuse break broken
REGRESS_TARGETS	+= empty emptyNAME emptyNAMEuse
REGRESS_TARGETS	+= font long punct

LINT_TARGETS	 = badNAME badNAMEuse break

# groff-1.22.3/mandoc differences:
# - When the first Nm does not have an argument but a later one has,
#   mandoc retroactively uses the later name for the earlier instances
#   of Nm, too, while groff does not.
# - When the head of an Nm block in the SYNOPSIS is broken by an
#   explicit block end macro on the same line, formatting differs,
#   but doesn't make sense either way.

SKIP_GROFF	 = badNAMEuse emptyNAMEuse broken
SKIP_TMAN	 = broken SILENT

# groff-1.22.3 defect:
# When a SYNOPSIS Nm block head breaks a sub block, all the
# remaining content in the document gets lost.

SKIP_GROFF	+= break

.include <bsd.regress.mk>
