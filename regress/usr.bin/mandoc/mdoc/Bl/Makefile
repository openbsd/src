# $OpenBSD: Makefile,v 1.16 2013/10/06 13:27:48 schwarze Exp $

REGRESS_TARGETS  = item inset diag ohang bullet dash enum hang tag
REGRESS_TARGETS += column extend nested

REGRESS_TARGETS += multitype multitag empty noIt bareTa unclosed break broken

# groff-1.20.1 defects:
# - empty lists ruin indentation and sometimes cause empty lines
# - breaking lists continue indefinitely
# - breaking a list aborts processing

SKIP_GROFF ?= empty break broken

SKIP_TMAN ?= column multitype multitag bareTa break broken

.include <bsd.regress.mk>
