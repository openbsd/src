# $OpenBSD: Makefile,v 1.6 2014/07/06 19:08:57 schwarze Exp $

REGRESS_TARGETS=escape infinite zerolength name
LINT_TARGETS = name

# The infinite test fails badly with groff-1.20.1:
# It fails to print the following text.

SKIP_GROFF ?= infinite

.include <bsd.regress.mk>
