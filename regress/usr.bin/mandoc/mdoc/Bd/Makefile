# $OpenBSD: Makefile,v 1.8 2012/11/18 17:59:03 schwarze Exp $

REGRESS_TARGETS = blank nested spacing  empty offset-empty  break broken

# groff-1.20.1 defect:    
# - a display breaking another block continues indefinitely

SKIP_GROFF = break

SKIP_TMAN = break broken

.include <bsd.regress.mk>
