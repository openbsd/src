# $OpenBSD: Makefile,v 1.2 2014/08/18 16:26:13 schwarze Exp $

REGRESS_TARGETS	= broken broken_eline empty_before noarg
LINT_TARGETS	= broken broken_eline empty_before noarg

# groff-1.22.2/mandoc differences:
#  * .SH without arguments just before EOF causes
#    two additional blank lines in groff.  [broken broken_eline]
#  * Vertical spacing is completely different
#    for empty .SH and .SS heads.  [noarg]

SKIP_GROFF	= broken broken_eline noarg

.include <bsd.regress.mk>
