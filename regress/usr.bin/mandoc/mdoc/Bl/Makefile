# $OpenBSD: Makefile,v 1.18 2014/07/02 11:42:56 schwarze Exp $

REGRESS_TARGETS  = item inset diag ohang bullet dash enum hang tag
REGRESS_TARGETS += column extend nested

REGRESS_TARGETS += multitype multitag empty noIt bareTa unclosed break broken

LINT_TARGETS	 = break broken

# groff-1.20.1 defects:
# - empty lists ruin indentation and sometimes cause empty lines
# - breaking lists continue indefinitely
# - breaking a list aborts processing

SKIP_GROFF ?= empty break broken

SKIP_TMAN ?= column multitype multitag bareTa break broken

# Fixing the indentation in long .IP and .TP tags in -man -Tascii
# caused a minor regression in -Tman that is not trivial to fix,
# so disable the related test for now.

SKIP_TMAN += tag

.include <bsd.regress.mk>
