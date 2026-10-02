# $OpenBSD: Makefile,v 1.12 2026/10/02 16:48:01 schwarze Exp $

REGRESS_TARGETS	 = badsec case dupe fourargs late
REGRESS_TARGETS	+= missing noarg nobody nosec order secsuffix
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

# groff-1.23.0 regressions:
#  - An .Os macro now terminates the prologue, in the sense that the last
#    .Dt macro before the first .Os macro governs the header, rather than
#    the last .Dt macro before the first non-prologue macro.
#  - The footer uses the absolute last .Dt macro, which is inconsistent
#    with what the header uses.

SKIP_GROFF	 = dupe late order

# groff-1.22.4 to 1.25.0 does not recognise section numbers with suffixes

SKIP_GROFF	+= secsuffix

.include <bsd.regress.mk>
