# $OpenBSD: Makefile,v 1.2 2020/09/03 20:33:20 schwarze Exp $

REGRESS_TARGETS	= basic-mdoc range

# groff-1.22.4 defect:
# - Negative page objects sometimes result in output lines starting
#   with backspace characters.
# - Excessive page offsets cause excessive line lengths
#   instead of being truncated to resonable widths.

SKIP_GROFF	 = range

.include <bsd.regress.mk>
