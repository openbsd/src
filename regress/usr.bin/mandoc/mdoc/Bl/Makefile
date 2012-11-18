# $OpenBSD: Makefile,v 1.14 2012/11/18 17:59:03 schwarze Exp $

REGRESS_TARGETS  = item inset diag ohang bullet dash enum hang tag
REGRESS_TARGETS += column extend nested

REGRESS_TARGETS += multitype multitag empty unclosed break broken

# groff-1.20.1 defects:
# - empty lists ruin indentation and sometimes cause empty lines
# - breaking lists continue indefinitely
# - breaking a list aborts processing

SKIP_GROFF ?= empty break broken

SKIP_TMAN ?= column multitype multitag break broken

.include <bsd.regress.mk>
