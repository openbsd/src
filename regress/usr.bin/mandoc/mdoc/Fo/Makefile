# $OpenBSD: Makefile,v 1.3 2011/11/17 16:28:45 schwarze Exp $

REGRESS_TARGETS=section noarg

# groff 1.21 does not handle .nr nS

SKIP_GROFF ?= section

SKIP_TMAN ?= ALL

.include <bsd.regress.mk>
