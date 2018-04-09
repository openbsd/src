# $OpenBSD: Makefile,v 1.8 2018/04/09 02:31:37 schwarze Exp $

REGRESS_TARGETS	 = escape infinite name std undef zerolength
LINT_TARGETS	 = name std undef

# The infinite test fails badly with groff-1.20.1:
# It fails to print the following text.

SKIP_GROFF	?= infinite

# Groff can expand standard macros as strings, but mandoc cannot.

SKIP_GROFF	+= std

.include <bsd.regress.mk>
