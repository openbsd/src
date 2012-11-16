# $OpenBSD: Makefile,v 1.7 2012/11/16 13:25:34 schwarze Exp $

REGRESS_TARGETS = empty blank offset-empty spacing break broken

# groff-1.20.1 defect:    
# - a display breaking another block continues indefinitely

SKIP_GROFF = break

SKIP_TMAN = break broken

.include <bsd.regress.mk>
