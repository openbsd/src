# $OpenBSD: Makefile,v 1.28 2015/02/06 02:04:35 schwarze Exp $

REGRESS_TARGETS  = item inset diag ohang bullet dash enum hang tag
REGRESS_TARGETS += column extend nested offset secstart

REGRESS_TARGETS += notype multitype badargs
REGRESS_TARGETS += empty noIt emptyhead emptytag emptyitem multitag
REGRESS_TARGETS += bareIt bareTa unclosed break breakingIt broken

LINT_TARGETS	 = column notype badargs
LINT_TARGETS	+= empty noIt emptyhead emptytag emptyitem
LINT_TARGETS	+= bareIt bareTa break breakingIt broken

# groff-1.22.3 defects:
# - lists with missing or late type ruin indentation
# - empty lists ruin indentation and sometimes cause empty lines
# - breaking lists continue indefinitely
# - breaking items sometimes ruin indentation, sometimes abort processing
# - breaking a list aborts processing
# - empty -tag item heads lose the blank line and the indentation

SKIP_GROFF ?= notype empty break breakingIt broken emptytag

SKIP_TMAN ?= column multitype multitag bareTa break broken

# Fixing the indentation in long .IP and .TP tags in -man -Tascii
# caused a minor regression in -Tman that is not trivial to fix,
# so disable the related test for now.

SKIP_TMAN += tag

# Empty heads are still mishandled by -Tman.

SKIP_TMAN += emptyhead emptytag

.include <bsd.regress.mk>
