# $OpenBSD: Makefile,v 1.3 2025/07/16 15:51:27 schwarze Exp $

REGRESS_TARGETS	= basic-mdoc fraction range

# groff-1.23.0 defects:
# - Negative page objects sometimes result in output lines starting
#   with backspace characters.
# - Excessive page offsets cause excessive line lengths
#   instead of being truncated to resonable widths.

SKIP_GROFF	 = range

.include <bsd.regress.mk>
