# $OpenBSD: Makefile,v 1.7 2015/02/04 18:03:28 schwarze Exp $

REGRESS_TARGETS	 = allch args break empty three_authors
LINT_TARGETS	 = allch args empty

# groff-1.22.3 defect:
# - arguments after .Rs cause the macro to be ignored

SKIP_GROFF	 = args

.include <bsd.regress.mk>
