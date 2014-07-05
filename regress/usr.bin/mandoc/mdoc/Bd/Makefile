# $OpenBSD: Makefile,v 1.13 2014/07/05 12:33:54 schwarze Exp $

REGRESS_TARGETS	 = blank nested spacing
REGRESS_TARGETS	+= badargs empty offset-empty
REGRESS_TARGETS	+= break broken

LINT_TARGETS	 = nested badargs break broken

# groff defect:
# - a display breaking another block continues indefinitely

SKIP_GROFF = break

.include <bsd.regress.mk>
