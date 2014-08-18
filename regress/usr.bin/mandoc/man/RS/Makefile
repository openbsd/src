# $OpenBSD: Makefile,v 1.8 2014/08/18 16:26:13 schwarze Exp $

REGRESS_TARGETS	 = breaking broken empty literal lonelyRE nested noRE width
LINT_TARGETS	 = broken lonelyRE noRE

# groff-1.22.2/mandoc difference:
# RS PP RE gives a blank line in groff, none in mandoc.

SKIP_GROFF	 = broken

.include <bsd.regress.mk>
