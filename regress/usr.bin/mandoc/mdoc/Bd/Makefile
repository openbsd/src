# $OpenBSD: Makefile,v 1.17 2014/11/10 20:59:41 schwarze Exp $

REGRESS_TARGETS	 = beforeNAME blank centered nested spacing
REGRESS_TARGETS	+= badargs empty offset-empty
REGRESS_TARGETS	+= break broken unclosed

LINT_TARGETS	 = beforeNAME blank nested badargs break broken unclosed

SKIP_TMAN	 = beforeNAME centered

# groff-1.22.2 defect:
# - a display breaking another block continues indefinitely

SKIP_GROFF	 = break

# adjustment is disabled in groff for now

SKIP_GROFF	+= centered

# groff handles content before NAME differently

SKIP_GROFF	+= beforeNAME

.include <bsd.regress.mk>
