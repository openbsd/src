# $OpenBSD: Makefile,v 1.10 2017/03/08 22:53:35 schwarze Exp $

REGRESS_TARGETS	 = allch args break empty three_authors
UTF8_TARGETS	 = allch break empty three_authors
LINT_TARGETS	 = allch args empty

# groff-1.22.3 defect:
# - arguments after .Rs cause the macro to be ignored

SKIP_GROFF	 = args

.include <bsd.regress.mk>
