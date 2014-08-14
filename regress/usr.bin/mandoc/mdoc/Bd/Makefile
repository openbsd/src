# $OpenBSD: Makefile,v 1.16 2014/08/14 02:00:52 schwarze Exp $

REGRESS_TARGETS	 = blank centered nested spacing
REGRESS_TARGETS	+= badargs empty offset-empty
REGRESS_TARGETS	+= break broken unclosed

LINT_TARGETS	 = blank nested badargs break broken unclosed

SKIP_TMAN	 = centered

# groff-1.22.2 defect:
# - a display breaking another block continues indefinitely

SKIP_GROFF	 = break

# adjustment is disabled in groff for now

SKIP_GROFF	+= centered

.include <bsd.regress.mk>
