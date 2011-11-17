# $OpenBSD: Makefile,v 1.5 2011/11/17 16:28:46 schwarze Exp $

REGRESS_TARGETS=escape infinite zerolength name

# The infinite test fails badly with groff-1.20.1:
# It fails to print the following text.

SKIP_GROFF ?= infinite

.include <bsd.regress.mk>
