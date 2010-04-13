# $OpenBSD: Makefile,v 1.4 2010/04/13 22:49:47 schwarze Exp $

REGRESS_TARGETS=tag inset hang diag column multitag nested
GROFF_TARGETS=tag inset hang diag column multitag nested

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
