# $OpenBSD: Makefile,v 1.1 2026/08/20 12:56:15 schwarze Exp $

REGRESS_TARGETS	 = man man-nogroff

# groff-1.23.0 defect:
# When there is a vertical line at the beginning of the first layout row
# and a horizontal line in any data cell of the first table span,
# GNU eqn(1) prints a vertical line to the output line before the table.
SKIP_GROFF	 = man-nogroff

.include <bsd.regress.mk>
