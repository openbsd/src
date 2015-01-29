# $OpenBSD: Makefile,v 1.1.1.1 2015/01/29 23:24:24 schwarze Exp $

REGRESS_TARGETS	 = expand font misalign

# groff-1.22.3 defect:
# - When space is insufficient (on either side) for properly aligning
#   a number, GNU tbl(1) moves the number too much to the right,
#   overflowing the column, even if space would be sufficient without
#   left padding.

SKIP_GROFF	 = misalign

.include <bsd.regress.mk>
