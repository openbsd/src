# $OpenBSD: Makefile,v 1.3 2010/04/12 22:57:05 schwarze Exp $

REGRESS_TARGETS=tag inset hang diag column nested
GROFF_TARGETS=tag inset hang diag column nested

REGRESS_TARGETS+=EMULTILIST ENOTYPE
REGRESS_SLOW_TARGETS=EMULTILIST

# broken:
# different rendering of bullet
# and different indentation
#REGRESS_TARGETS+=bullet
#GROFF_TARGETS+=bullet.out_ascii

# broken:
# different indentation
#REGRESS_TARGETS+=enum
#GROFF_TARGETS+=enum.out_ascii

.include <bsd.regress.mk>
