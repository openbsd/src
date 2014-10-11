# $OpenBSD: Makefile,v 1.10 2014/10/11 21:14:11 schwarze Exp $

REGRESS_TARGETS = basic break eos font noarg obsolete section warn
LINT_TARGETS	= noarg obsolete warn

# groff does not handle .nr nS

SKIP_GROFF ?= section

SKIP_TMAN = eos

.include <bsd.regress.mk>
