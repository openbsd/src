# $OpenBSD: Makefile,v 1.16 2017/06/10 01:48:31 schwarze Exp $

REGRESS_TARGETS	 = basic break eos font noarg nohead
REGRESS_TARGETS += obsolete punct section warn
LINT_TARGETS	 = noarg nohead obsolete punct warn

# groff-1.22.3 defects:
# - .Fo without an argument prints unbalanced parentheses
# - .nr nS is ignored

SKIP_GROFF	 = nohead section

SKIP_TMAN = eos

.include <bsd.regress.mk>
