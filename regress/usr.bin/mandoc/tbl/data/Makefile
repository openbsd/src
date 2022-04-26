# $OpenBSD: Makefile,v 1.6 2022/04/26 14:46:30 schwarze Exp $

REGRESS_TARGETS	 = blankline block_empty block_unclosed block_width
REGRESS_TARGETS	+= block_wrap empty escz escz_end insert
LINT_TARGETS	 = block_unclosed empty insert

# groff-1.23.0 defects:
# - When a table ends in the middle of a block,
#   GNU eqn produces no output whatsoever for the whole table.
# - When a table cell ends with a no-advance escape sequence,
#   the next cell is appended to the current cell instead of
#   aligning it to the next column.

SKIP_GROFF	 = block_unclosed escz_end

.include <bsd.regress.mk>
