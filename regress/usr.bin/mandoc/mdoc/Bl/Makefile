# $OpenBSD: Makefile,v 1.2 2009/12/24 23:29:44 schwarze Exp $

REGRESS_TARGETS=tag inset hang diag column
GROFF_TARGETS=tag inset hang diag column

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
