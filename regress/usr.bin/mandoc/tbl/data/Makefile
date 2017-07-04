# $OpenBSD: Makefile,v 1.4 2017/07/04 20:59:17 schwarze Exp $

REGRESS_TARGETS  = blankline block_unclosed block_width block_wrap empty insert
LINT_TARGETS	 = block_unclosed empty insert

# groff-1.22.3 defect:
# - When a table ends in the middle of a block,
#   GNU eqn produces no output whatsoever for the whole table.

SKIP_GROFF	 = block_unclosed

.include <bsd.regress.mk>
