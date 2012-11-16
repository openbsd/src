# $OpenBSD: Makefile,v 1.13 2012/11/16 13:25:34 schwarze Exp $

REGRESS_TARGETS  = item inset diag ohang bullet dash enum hang tag
REGRESS_TARGETS += column extend nested

REGRESS_TARGETS += multitype multitag empty unclosed break broken

# groff-1.20.1 defects:
# - empty lists ruin indentation and sometimes cause empty lines
# - breaking lists continue indefinitely
# - breaking a list aborts processing

SKIP_GROFF ?= empty break broken

SKIP_TMAN ?= column nested multitype multitag break broken

.include <bsd.regress.mk>
