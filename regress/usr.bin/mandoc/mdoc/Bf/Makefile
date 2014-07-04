# $OpenBSD: Makefile,v 1.4 2014/07/04 16:11:42 schwarze Exp $

REGRESS_TARGETS	= nest multiargs badargs break broken
LINT_TARGETS	= multiargs badargs break broken

# groff-1.22.2 defects:
#  - If the first argument to .Bf is a bad one,
#    no block is opened at all and the .Ef causes another error.
#  - If a font block breaks another block,
#    fonts get mixed up when the other block finally ends.

SKIP_GROFF = badargs break

SKIP_TMAN = break broken

.include <bsd.regress.mk>
