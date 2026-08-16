# $OpenBSD: Makefile,v 1.11 2026/08/16 16:22:47 schwarze Exp $
#
# This directory is intended for tests of string *expansion*,
# in particular testing the behaviour of the \* escape sequence.
# Tests of string *definitions* are better placed in the roff/ds
# directory.

REGRESS_TARGETS	 = dotT escape infinite name std stringup stringup_bad
REGRESS_TARGETS	+= undef zerolength
LINT_TARGETS	 = name std stringup_bad undef
UTF8_TARGETS	 = dotT stringup
HTML_TARGETS	 = dotT
SKIP_MARKDOWN	 = escape infinite name std stringup stringup_bad
SKIP_MARKDOWN	+= undef zerolength

# The infinite test fails badly with groff-1.20.1:
# It fails to print the following text.

SKIP_GROFF	?= infinite

# groff-1.24 lets excess .stringup arguments sneak through to the output stream

SKIP_GROFF	+= stringup_bad

# Groff can expand standard macros as strings, but mandoc cannot.

SKIP_GROFF	+= std

.include <bsd.regress.mk>
