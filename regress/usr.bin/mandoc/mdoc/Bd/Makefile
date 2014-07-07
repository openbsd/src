# $OpenBSD: Makefile,v 1.15 2014/07/07 21:35:42 schwarze Exp $

REGRESS_TARGETS	 = blank nested spacing
REGRESS_TARGETS	+= badargs empty offset-empty
REGRESS_TARGETS	+= break broken unclosed

LINT_TARGETS	 = blank nested badargs break broken unclosed

# groff-1.22.2 defect:
# - a display breaking another block continues indefinitely

SKIP_GROFF = break

.include <bsd.regress.mk>
