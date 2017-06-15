# $OpenBSD: Makefile,v 1.3 2017/06/15 00:27:23 schwarze Exp $

REGRESS_TARGETS	 = blankline block_unclosed block_width block_wrap empty
LINT_TARGETS	 = block_unclosed empty

# groff-1.22.3 defect:
# - When a table ends in the middle of a block,
#   GNU eqn produces no output whatsoever for the whole table.

SKIP_GROFF	 = block_unclosed

.include <bsd.regress.mk>
