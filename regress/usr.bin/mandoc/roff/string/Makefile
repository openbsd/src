# $OpenBSD: Makefile,v 1.10 2022/06/03 11:50:25 schwarze Exp $
#
# This directory is intended for tests of string *expansion*,
# in particular testing the behaviour of the \* escape sequence.
# Tests of string *definitions* are better placed in the roff/ds
# directory.

REGRESS_TARGETS	 = dotT escape infinite name std undef zerolength
LINT_TARGETS	 = name std undef
UTF8_TARGETS	 = dotT
HTML_TARGETS	 = dotT
SKIP_MARKDOWN	 = escape infinite name std undef zerolength

# The infinite test fails badly with groff-1.20.1:
# It fails to print the following text.

SKIP_GROFF	?= infinite

# Groff can expand standard macros as strings, but mandoc cannot.

SKIP_GROFF	+= std

.include <bsd.regress.mk>
