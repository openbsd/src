# $OpenBSD: Makefile,v 1.8 2018/12/21 16:58:49 schwarze Exp $

REGRESS_TARGETS	 = arg break empty obsolete unclosed
UTF8_TARGETS	 = arg
LINT_TARGETS	 = break obsolete unclosed

# groff-1.22.3 defect:
# - When .Ec is missing, groff prints no page footer.

SKIP_GROFF	 = unclosed

.include <bsd.regress.mk>
