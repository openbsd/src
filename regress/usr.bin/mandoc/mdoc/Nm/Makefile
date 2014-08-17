# $OpenBSD: Makefile,v 1.9 2014/08/17 18:42:07 schwarze Exp $

REGRESS_TARGETS	 = badNAME badNAMEuse break empty emptyNAME emptyNAMEuse
REGRESS_TARGETS	+= font long

LINT_TARGETS	 = badNAME badNAMEuse break

# groff-1.22.2/mandoc difference:
# When the first Nm does not have an argument but a later one has,
# mandoc retroactively uses the later name for the earlier instances
# of Nm, too, while groff does not.

SKIP_GROFF	 = badNAMEuse emptyNAMEuse

# groff-1.22.2 defect:
# When a SYNOPSIS Nm block head breaks a sub block, all the
# remaining content in the document gets lost.

SKIP_GROFF	+= break

.include <bsd.regress.mk>
