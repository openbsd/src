# $OpenBSD: Makefile,v 1.3 2025/07/16 16:36:03 schwarze Exp $

REGRESS_TARGETS	 = basic-mdoc basic-man fraction wide

# groff-1.23.0 defect:
# - Excessive temporary indentations cause excessive line lengths
#   instead of being truncated to resonable indentations.

SKIP_GROFF	 = wide

.include <bsd.regress.mk>
