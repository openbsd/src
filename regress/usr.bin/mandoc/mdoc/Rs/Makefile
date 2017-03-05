# $OpenBSD: Makefile,v 1.9 2017/03/05 19:59:26 schwarze Exp $

REGRESS_TARGETS	 = allch args break empty three_authors
UTF8_TARGETS	 = allch break empty three_authors
LINT_TARGETS	 = allch args empty
MARKDOWN_TARGETS = allch args break empty three_authors

# groff-1.22.3 defect:
# - arguments after .Rs cause the macro to be ignored

SKIP_GROFF	 = args

.include <bsd.regress.mk>
