# $OpenBSD: Makefile,v 1.12 2015/02/06 03:31:11 schwarze Exp $

REGRESS_TARGETS	 = basic break eos font noarg nohead
REGRESS_TARGETS += obsolete punct section warn
LINT_TARGETS	 = noarg nohead obsolete warn

# groff-1.22.3 defects:
# - .Fo without an argument prints unbalanced parentheses
# - .nr nS is ignored

SKIP_GROFF	 = noarg section

SKIP_TMAN = eos

.include <bsd.regress.mk>
