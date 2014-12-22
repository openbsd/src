# $OpenBSD: Makefile,v 1.1 2014/12/22 23:26:20 schwarze Exp $

REGRESS_TARGETS	= twice
LINT_TARGETS	= twice

# groff-1.22.3 defect:
# - non-matching enclosure end macro prints a closing delimiter

SKIP_GROFF	= twice

.include <bsd.regress.mk>
