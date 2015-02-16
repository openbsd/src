# $OpenBSD: Makefile,v 1.6 2015/02/16 19:02:32 schwarze Exp $

REGRESS_TARGETS	 = badsec case dupe fourargs late
REGRESS_TARGETS	+= missing noarg nobody nosec order
LINT_TARGETS	 = badsec case dupe fourargs late
LINT_TARGETS	+= missing noarg nobody nosec order

SKIP_GROFF	 = badsec nobody
SKIP_ASCII	 = badsec

# When the section is missing from the man(7) .TH title line,
# empty parentheses are shown in the page header.
# When the section is missing from the mdoc(7) .Dt title line,
# no empty parentheses are shown in the page header.
# Hence, there is no bug-compatible translation from mdoc(7)
# to man(7) for this case.  That's not mandoc(1)'s fault.

SKIP_TMAN	 = fourargs late missing noarg nosec

.include <bsd.regress.mk>
