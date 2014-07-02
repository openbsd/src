# $OpenBSD: Makefile,v 1.9 2014/07/02 20:18:42 schwarze Exp $

REGRESS_TARGETS = basic section break noarg font eos obsolete
LINT_TARGETS	= noarg obsolete

# groff does not handle .nr nS

SKIP_GROFF ?= section

SKIP_TMAN = eos

.include <bsd.regress.mk>
