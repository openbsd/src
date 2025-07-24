# $OpenBSD: Makefile,v 1.7 2025/07/24 17:04:57 schwarze Exp $

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

# groff-1.23.0 regression:
# - When a table does not contain any cells, it causes no output,
#   not even a line break.

SKIP_GROFF	+= empty

.include <bsd.regress.mk>
