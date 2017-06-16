# $OpenBSD: Makefile,v 1.3 2017/06/16 20:00:41 schwarze Exp $

REGRESS_TARGETS	 = center complex empty emptyline
REGRESS_TARGETS	+= lines lines-nogroff numbers span
LINT_TARGETS	 = complex empty

# groff-1.22.3 defects:
# - When the layout is completely empty,
#   GNU eqn produces no output whatsoever for the whole table.
# - When there is no horizontal line at the top,
#   vertical lines extend beyond the top of the table.

SKIP_GROFF	 = empty lines-nogroff

.include <bsd.regress.mk>
