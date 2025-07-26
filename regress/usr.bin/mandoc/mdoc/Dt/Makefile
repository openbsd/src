# $OpenBSD: Makefile,v 1.9 2025/07/26 12:32:01 schwarze Exp $

REGRESS_TARGETS	 = badsec case dupe fourargs late
REGRESS_TARGETS	+= missing noarg nobody nosec order
LINT_TARGETS	 = badsec case dupe fourargs late
LINT_TARGETS	+= missing noarg nobody nosec order

SKIP_GROFF	 = nobody

# When the section is missing from the man(7) .TH title line,
# empty parentheses are shown in the page header.
# When the section is missing from the mdoc(7) .Dt title line,
# no empty parentheses are shown in the page header.
# Hence, there is no bug-compatible translation from mdoc(7)
# to man(7) for this case.  That's not mandoc(1)'s fault.

SKIP_TMAN	 = fourargs late missing noarg nosec

.include <bsd.regress.mk>
