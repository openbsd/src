# $OpenBSD: Makefile,v 1.18 2020/03/13 00:31:06 schwarze Exp $

REGRESS_TARGETS	 = basic break eos font noarg nohead
REGRESS_TARGETS += obsolete punct section tag transp warn
TAG_TARGETS	 = tag
LINT_TARGETS	 = noarg nohead obsolete punct warn
HTML_TARGETS	 = tag

# groff-1.22.3 defects:
# - .Fo without an argument prints unbalanced parentheses
# - .nr nS is ignored

SKIP_GROFF	 = nohead section

SKIP_TMAN = eos

.include <bsd.regress.mk>
