# $OpenBSD: Makefile,v 1.10 2025/07/26 12:46:17 schwarze Exp $

REGRESS_TARGETS	 = badsec case dupe fourargs late
REGRESS_TARGETS	+= missing noarg nobody nosec order
LINT_TARGETS	 = badsec case dupe fourargs late
LINT_TARGETS	+= missing noarg nobody nosec order

# When the section is missing from the man(7) .TH title line,
# empty parentheses are shown in the page header.
# When the section is missing from the mdoc(7) .Dt title line,
# no empty parentheses are shown in the page header.
# Hence, there is no bug-compatible translation from mdoc(7)
# to man(7) for this case.  That's not mandoc(1)'s fault.

SKIP_TMAN	 = late missing noarg nosec

# The man(7) .TH macro does not support architecture arguments,
# so machine-dependent manuals cannot be converted faithfully.

SKIP_TMAN	+= fourargs

.include <bsd.regress.mk>
