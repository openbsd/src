# $OpenBSD: Makefile,v 1.11 2014/11/20 00:30:34 schwarze Exp $

REGRESS_TARGETS = basic break eos font noarg obsolete punct section warn
LINT_TARGETS	= noarg obsolete warn

# groff does not handle .nr nS

SKIP_GROFF ?= section

SKIP_TMAN = eos

.include <bsd.regress.mk>
