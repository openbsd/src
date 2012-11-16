# $OpenBSD: Makefile,v 1.2 2012/11/16 17:16:29 schwarze Exp $

REGRESS_TARGETS = nest break broken

# groff-1.20.1 defect:
#  - If a font block breaks another block,
#    fonts get mixed up when the other block finally ends.

SKIP_GROFF = break

SKIP_TMAN = break broken

.include <bsd.regress.mk>
