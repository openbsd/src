# $OpenBSD: Makefile,v 1.1 2009/10/28 03:20:42 schwarze Exp $

REGRESS_TARGETS=tag inset hang diag
GROFF_TARGETS=tag inset hang diag

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
