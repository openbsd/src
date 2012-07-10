# $OpenBSD: Makefile,v 1.11 2012/07/10 19:53:12 schwarze Exp $

REGRESS_TARGETS  = item inset diag ohang bullet dash enum hang tag
REGRESS_TARGETS += column extend nested

REGRESS_TARGETS+=multitype multitag empty unclosed

# groff-1.20.1 mishandles empty lists,
# ruining indentation and sometimes causing empty lines.

SKIP_GROFF ?= empty

SKIP_TMAN ?= tag column extend nested multitype multitag empty

.include <bsd.regress.mk>
