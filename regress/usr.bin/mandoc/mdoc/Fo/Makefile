# $OpenBSD: Makefile,v 1.8 2014/07/02 03:47:07 schwarze Exp $

REGRESS_TARGETS = basic section break noarg font eos obsolete

LINT_TARGETS = obsolete

# groff does not handle .nr nS

SKIP_GROFF ?= section

SKIP_TMAN = eos

.include <bsd.regress.mk>
