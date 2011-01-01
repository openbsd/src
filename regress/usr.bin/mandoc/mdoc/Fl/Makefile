# $OpenBSD: Makefile,v 1.4 2011/01/01 22:08:43 schwarze Exp $

REGRESS_TARGETS=noarg multiarg parsed punct
GROFF_TARGETS=noarg multiarg parsed

# Do not regenerate the punct output file for now,
# because groff-1.15 and groff-1.20.1 handle
# the bar differently as a .Fl argument.

.include <bsd.regress.mk>
