# $OpenBSD: Makefile,v 1.12 2014/07/05 01:11:33 schwarze Exp $

REGRESS_TARGETS	 = blank nested spacing
REGRESS_TARGETS	+= notype badargs empty offset-empty
REGRESS_TARGETS	+= break broken

LINT_TARGETS	 = nested notype badargs break broken

# groff defect:
# - a display breaking another block continues indefinitely

SKIP_GROFF = break

.include <bsd.regress.mk>
