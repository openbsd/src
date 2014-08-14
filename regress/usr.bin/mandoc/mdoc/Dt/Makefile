# $OpenBSD: Makefile,v 1.4 2014/08/14 02:00:53 schwarze Exp $

REGRESS_TARGETS	 = badarch badsec case dupe late
REGRESS_TARGETS	+= missing noarg nobody nosec order
LINT_TARGETS	 = badarch badsec case dupe late
LINT_TARGETS	+= missing noarg nobody nosec order

SKIP_GROFF	 = badarch badsec nobody
SKIP_ASCII	 = badarch badsec

# When the section is missing from the man(7) .TH title line,
# empty parentheses are shown in the page header.
# When the section is missing from the mdoc(7) .Dt title line,
# no empty parentheses are shown in the page header.
# Hence, there is no bug-compatible translation from mdoc(7)
# to man(7) for this case.  That's not mandoc(1)'s fault.

SKIP_TMAN	 = late missing noarg nosec

.include <bsd.regress.mk>
