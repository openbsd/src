# $OpenBSD: Makefile,v 1.9 2014/01/24 11:56:34 schwarze Exp $

REGRESS_TARGETS = blank nested spacing  empty offset-empty  break broken

# groff defect:
# - a display breaking another block continues indefinitely

SKIP_GROFF = break

.include <bsd.regress.mk>
