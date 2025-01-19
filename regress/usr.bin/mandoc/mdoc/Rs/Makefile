# $OpenBSD: Makefile,v 1.13 2025/01/19 23:01:27 schwarze Exp $

REGRESS_TARGETS	 = allch args break empty paragraph three_authors rfc transp
UTF8_TARGETS	 = allch break empty three_authors
LINT_TARGETS	 = allch args empty
HTML_TARGETS	 = paragraph rfc

# groff-1.22.3 defect:
# - arguments after .Rs cause the macro to be ignored

SKIP_GROFF	 = args

.include <bsd.regress.mk>
