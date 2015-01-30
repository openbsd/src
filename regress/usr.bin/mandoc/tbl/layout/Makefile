# $OpenBSD: Makefile,v 1.2 2015/01/30 00:27:09 schwarze Exp $

REGRESS_TARGETS	 = center complex empty emptyline numbers span
LINT_TARGETS	 = complex empty

# groff-1.22.3 defect:
# - When the layout is completely empty,
#   GNU eqn produces no output whatsoever for the whole table.

SKIP_GROFF	 = empty

.include <bsd.regress.mk>
