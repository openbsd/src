# $OpenBSD: Makefile,v 1.12 2012/07/10 20:36:33 schwarze Exp $

REGRESS_TARGETS  = item inset diag ohang bullet dash enum hang tag
REGRESS_TARGETS += column extend nested

REGRESS_TARGETS+=multitype multitag empty unclosed

# groff-1.20.1 mishandles empty lists,
# ruining indentation and sometimes causing empty lines.

SKIP_GROFF ?= empty

SKIP_TMAN ?= column nested multitype multitag

.include <bsd.regress.mk>
