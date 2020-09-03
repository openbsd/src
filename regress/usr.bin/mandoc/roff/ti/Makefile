# $OpenBSD: Makefile,v 1.2 2020/09/03 17:37:06 schwarze Exp $

REGRESS_TARGETS	 = basic-mdoc basic-man wide

# groff-1.22.4 defect:
# - Excessive temporare indentations cause excessive line lengths
#   instead of being truncated to resonable indentations.

SKIP_GROFF	 = wide

.include <bsd.regress.mk>
