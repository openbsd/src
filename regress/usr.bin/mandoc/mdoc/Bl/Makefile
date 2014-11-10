# $OpenBSD: Makefile,v 1.26 2014/11/10 21:54:29 schwarze Exp $

REGRESS_TARGETS  = item inset diag ohang bullet dash enum hang tag
REGRESS_TARGETS += column extend nested offset secstart

REGRESS_TARGETS += notype multitype badargs
REGRESS_TARGETS += empty noIt emptyhead emptytag emptyitem multitag
REGRESS_TARGETS += bareIt bareTa unclosed break broken

LINT_TARGETS	 = column notype badargs
LINT_TARGETS	+= noIt emptyhead emptytag emptyitem
LINT_TARGETS	+= bareIt bareTa break broken

# groff-1.22.2 defects:
# - lists with missing or late type ruin indentation
# - empty lists ruin indentation and sometimes cause empty lines
# - breaking lists continue indefinitely
# - breaking a list aborts processing
# - empty -tag item heads lose the blank line and the indentation

SKIP_GROFF ?= notype empty break broken emptytag

SKIP_TMAN ?= column multitype multitag bareTa break broken

# Fixing the indentation in long .IP and .TP tags in -man -Tascii
# caused a minor regression in -Tman that is not trivial to fix,
# so disable the related test for now.

SKIP_TMAN += tag

# Empty heads are still mishandled by -Tman.

SKIP_TMAN += emptyhead emptytag

.include <bsd.regress.mk>
