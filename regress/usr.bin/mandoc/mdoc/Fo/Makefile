# $OpenBSD: Makefile,v 1.14 2017/03/05 19:59:26 schwarze Exp $

REGRESS_TARGETS	 = basic break eos font noarg nohead
REGRESS_TARGETS += obsolete punct section warn
LINT_TARGETS	 = noarg nohead obsolete warn
MARKDOWN_TARGETS  = basic break eos font noarg nohead
MARKDOWN_TARGETS += obsolete punct section warn

# groff-1.22.3 defects:
# - .Fo without an argument prints unbalanced parentheses
# - .nr nS is ignored

SKIP_GROFF	 = nohead section

SKIP_TMAN = eos

.include <bsd.regress.mk>
