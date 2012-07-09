# $OpenBSD: Makefile,v 1.7 2012/07/09 17:52:09 schwarze Exp $

REGRESS_TARGETS = noarg multiarg parsed punct font

# Do not regenerate the punct output file for now,
# because groff-1.15 and groff-1.20.1 handle
# the bar differently as a .Fl argument.

SKIP_GROFF ?= punct

.include <bsd.regress.mk>
