# $OpenBSD: Makefile,v 1.15 2017/03/05 19:59:26 schwarze Exp $

REGRESS_TARGETS	 = badNAME badNAMEuse break broken
REGRESS_TARGETS	+= empty emptyNAME emptyNAMEuse
REGRESS_TARGETS	+= font long par parns punct

LINT_TARGETS	 = badNAME badNAMEuse break broken

MARKDOWN_TARGETS  = break broken empty
MARKDOWN_TARGETS += font long par parns punct

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

.include <bsd.regress.mk>
