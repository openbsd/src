# $OpenBSD: Makefile,v 1.40 2018/12/21 16:58:49 schwarze Exp $

REGRESS_TARGETS  = item inset diag ohang bullet dash enum hang tag
REGRESS_TARGETS += column column_nogroff colNoIt
REGRESS_TARGETS += esc extend nested offset secstart

REGRESS_TARGETS += notype multitype badargs
REGRESS_TARGETS += empty noIt emptyhead emptytag emptyitem multitag
REGRESS_TARGETS += bareIt bareTa unclosed break breakingIt breakingTa broken

UTF8_TARGETS	 = dash

LINT_TARGETS	 = inset diag column column_nogroff notype badargs tag
LINT_TARGETS	+= empty noIt emptyhead emptytag emptyitem
LINT_TARGETS	+= bareIt bareTa break breakingIt broken

# groff-mandoc differences:
# - groff allows enclosures to span Ta, mandoc does not

SKIP_GROFF ?= breakingTa

# groff-1.22.3 defects:
# - column list items with no args but multiple lines cause bogus breaks
# - in column lists, the tab macro cannot be a line macro
# - lists with missing or late type ruin indentation
# - empty lists ruin indentation and sometimes cause empty lines
# - breaking lists continue indefinitely
# - breaking items sometimes ruin indentation, sometimes abort processing
# - breaking a list aborts processing
# - empty -tag item heads lose the blank line and the indentation

SKIP_GROFF += column_nogroff notype empty emptytag break breakingIt broken

SKIP_TMAN ?= column colNoIt multitype multitag bareTa break breakingTa broken

# Fixing the indentation in long .IP and .TP tags in -man -Tascii
# caused a minor regression in -Tman that is not trivial to fix,
# so disable the related test for now.

SKIP_TMAN += tag

# Empty heads are still mishandled by -Tman.

SKIP_TMAN += column_nogroff emptyhead emptytag

# mandoc -T markdown still has issues with badly nested lists

SKIP_MARKDOWN ?= break

.include <bsd.regress.mk>
