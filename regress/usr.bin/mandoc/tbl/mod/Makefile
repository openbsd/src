# $OpenBSD: Makefile,v 1.5 2017/06/27 19:51:28 schwarze Exp $

REGRESS_TARGETS	 = badfont expand expand-toowide font misalign spacing width
LINT_TARGETS	 = badfont font

# groff-1.22.3 defects:
# - The "f" font modifier swallows a following newline character.
# - When space is insufficient (on either side) for properly aligning
#   a number, GNU tbl(1) moves the number too much to the right,
#   overflowing the column, even if space would be sufficient without
#   left padding.

SKIP_GROFF	 = badfont misalign

.include <bsd.regress.mk>
