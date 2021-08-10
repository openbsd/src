# $OpenBSD: Makefile,v 1.6 2021/08/10 12:36:42 schwarze Exp $

REGRESS_TARGETS	 = badfont expand expand-toowide font font-eol
REGRESS_TARGETS	+= misalign spacing width
LINT_TARGETS	 = badfont font font-eol

# groff-1.22.4 defects:
# - The "f" font modifier swallows a following newline character.
# - When space is insufficient (on either side) for properly aligning
#   a number, GNU tbl(1) moves the number too much to the right,
#   overflowing the column, even if space would be sufficient without
#   left padding.

SKIP_GROFF	 = font-eol misalign

.include <bsd.regress.mk>
