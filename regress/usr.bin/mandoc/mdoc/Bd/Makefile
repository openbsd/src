# $OpenBSD: Makefile,v 1.11 2014/07/04 01:50:03 schwarze Exp $

REGRESS_TARGETS	 = blank nested spacing
REGRESS_TARGETS	+= notype empty offset-empty
REGRESS_TARGETS	+= break broken

LINT_TARGETS	 = nested notype break broken

# groff defect:
# - a display breaking another block continues indefinitely

SKIP_GROFF = break

.include <bsd.regress.mk>
