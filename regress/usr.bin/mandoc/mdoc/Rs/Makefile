# $OpenBSD: Makefile,v 1.11 2019/01/07 06:51:37 schwarze Exp $

REGRESS_TARGETS	 = allch args break empty paragraph three_authors
UTF8_TARGETS	 = allch break empty three_authors
LINT_TARGETS	 = allch args empty
HTML_TARGETS	 = paragraph

# groff-1.22.3 defect:
# - arguments after .Rs cause the macro to be ignored

SKIP_GROFF	 = args

.include <bsd.regress.mk>
