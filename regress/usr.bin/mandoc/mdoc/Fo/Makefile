# $OpenBSD: Makefile,v 1.7 2013/12/25 00:39:14 schwarze Exp $

REGRESS_TARGETS = basic section break noarg font eos

# groff does not handle .nr nS

SKIP_GROFF ?= section

SKIP_TMAN = eos

.include <bsd.regress.mk>
