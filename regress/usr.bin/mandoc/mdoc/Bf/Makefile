# $OpenBSD: Makefile,v 1.3 2014/07/02 11:42:56 schwarze Exp $

REGRESS_TARGETS = nest break broken
LINT_TARGETS	= break broken

# groff-1.20.1 defect:
#  - If a font block breaks another block,
#    fonts get mixed up when the other block finally ends.

SKIP_GROFF = break

SKIP_TMAN = break broken

.include <bsd.regress.mk>
