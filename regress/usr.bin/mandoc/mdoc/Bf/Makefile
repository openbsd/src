# $OpenBSD: Makefile,v 1.6 2019/01/07 06:51:37 schwarze Exp $

REGRESS_TARGETS	 = badargs break broken multiargs nest paragraph
LINT_TARGETS	 = badargs break broken multiargs
HTML_TARGETS	 = paragraph

# mandoc -T markdown ignores .Bf, at least for now

SKIP_MARKDOWN ?= ALL

# groff-1.22.2 defects:
#  - If the first argument to .Bf is a bad one,
#    no block is opened at all and the .Ef causes another error.
#  - If a font block breaks another block,
#    fonts get mixed up when the other block finally ends.

SKIP_GROFF = badargs break

SKIP_TMAN = break broken

.include <bsd.regress.mk>
