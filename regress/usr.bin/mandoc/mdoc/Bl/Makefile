# $OpenBSD: Makefile,v 1.7 2011/01/09 18:15:51 schwarze Exp $

REGRESS_TARGETS=tag inset hang diag column extend nested
GROFF_TARGETS=tag inset diag column extend nested

# mandoc still contains a compatibility hack with groff-1.15
# regarding hang list indentation.

REGRESS_TARGETS+=multitype multitag empty unclosed
GROFF_TARGETS+=multitype multitag unclosed

# groff-1.20.1 mishandles empty lists,
# ruining indentation and sometimes cauisng empty lines.

REGRESS_TARGETS+=EMULTILIST
REGRESS_SLOW_TARGETS=EMULTILIST

.include <bsd.regress.mk>
