# $OpenBSD: Makefile,v 1.20 2014/07/03 23:23:45 schwarze Exp $

REGRESS_TARGETS  = item inset diag ohang bullet dash enum hang tag
REGRESS_TARGETS += column extend nested

REGRESS_TARGETS += multitype multitag
REGRESS_TARGETS += empty noIt emptyhead emptytag
REGRESS_TARGETS += bareTa unclosed break broken

LINT_TARGETS	 = noIt emptyhead emptytag break broken

# groff-1.22.2 defects:
# - empty lists ruin indentation and sometimes cause empty lines
# - breaking lists continue indefinitely
# - breaking a list aborts processing
# - empty -tag item heads lose the blank line and the indentation

SKIP_GROFF ?= empty break broken emptytag

SKIP_TMAN ?= column multitype multitag bareTa break broken

# Fixing the indentation in long .IP and .TP tags in -man -Tascii
# caused a minor regression in -Tman that is not trivial to fix,
# so disable the related test for now.

SKIP_TMAN += tag

# Empty heads are still mishandled by -Tman.

SKIP_TMAN += emptyhead emptytag

.include <bsd.regress.mk>
