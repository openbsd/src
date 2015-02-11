# $OpenBSD: Makefile,v 1.4 2015/02/11 14:14:53 schwarze Exp $

REGRESS_TARGETS	 = break empty obsolete unclosed
LINT_TARGETS	 = break obsolete unclosed

# groff-1.22.3 defect:
# - When .Ec is missing, groff prints no page footer.

SKIP_GROFF	 = unclosed

.include <bsd.regress.mk>
