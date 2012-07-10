# $OpenBSD: Makefile,v 1.9 2012/07/10 14:35:57 schwarze Exp $

REGRESS_TARGETS  = item inset diag ohang bullet dash enum hang tag
REGRESS_TARGETS += column extend nested

# mandoc still contains a compatibility hack with groff-1.15
# regarding hang list indentation.

REGRESS_TARGETS+=multitype multitag empty unclosed

# groff-1.20.1 mishandles empty lists,
# ruining indentation and sometimes cauisng empty lines.

SKIP_GROFF ?= hang empty

SKIP_TMAN ?= tag hang column extend nested multitype multitag empty

.include <bsd.regress.mk>
