# $OpenBSD: Makefile,v 1.7 2017/06/18 17:35:40 schwarze Exp $

REGRESS_TARGETS	 = escape infinite name std zerolength
LINT_TARGETS	 = name std

# The infinite test fails badly with groff-1.20.1:
# It fails to print the following text.

SKIP_GROFF	?= infinite

# Groff can expand standard macros as strings, but mandoc cannot.

SKIP_GROFF	+= std

.include <bsd.regress.mk>
