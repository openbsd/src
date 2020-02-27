# $OpenBSD: Makefile,v 1.12 2020/02/27 01:25:58 schwarze Exp $

REGRESS_TARGETS	 = allch args break empty paragraph three_authors transp
UTF8_TARGETS	 = allch break empty three_authors
LINT_TARGETS	 = allch args empty
HTML_TARGETS	 = paragraph

# groff-1.22.3 defect:
# - arguments after .Rs cause the macro to be ignored

SKIP_GROFF	 = args

.include <bsd.regress.mk>
