# $OpenBSD: Makefile,v 1.6 2017/03/07 14:03:56 schwarze Exp $

REGRESS_TARGETS	 = break empty obsolete unclosed
LINT_TARGETS	 = break obsolete unclosed
MARKDOWN_TARGETS = break empty obsolete unclosed

# groff-1.22.3 defect:
# - When .Ec is missing, groff prints no page footer.

SKIP_GROFF	 = unclosed

.include <bsd.regress.mk>
