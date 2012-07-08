# $OpenBSD: Makefile,v 1.4 2012/07/08 18:38:07 schwarze Exp $

REGRESS_TARGETS = basic section noarg

# groff 1.21 does not handle .nr nS

SKIP_GROFF ?= section

.include <bsd.regress.mk>
