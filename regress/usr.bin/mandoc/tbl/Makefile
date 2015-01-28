# $OpenBSD: Makefile,v 1.12 2015/01/28 17:30:37 schwarze Exp $

REGRESS_TARGETS	 = blankline block_unclosed center
REGRESS_TARGETS	+= empty expand fonts layout layout_center
REGRESS_TARGETS	+= macro misalign nested numbers opt span vert
LINT_TARGETS	 = block_unclosed empty layout macro nested opt

# groff-1.22.3 defects:
# - When a table ends in the middle of a block,
#   GNU eqn produces no output whatsoever for the whole table.
# - When space is insufficient (on either side) for properly aligning
#   a number, GNU tbl(1) moves the number too much to the right,
#   overflowing the column, even if space would be sufficient without
#   left padding.

# trivial difference to groff-1.22.3:
# .TS in a table causes a blank table line in GNU tbl(1), but not in mandoc.

SKIP_GROFF	 = block_unclosed misalign nested
SKIP_TMAN	?= ALL SILENT

TBL = /usr/local/bin/tbl

.for t in ${REGRESS_TARGETS}
${t}.out_ascii: ${t}.in
	${TBL} ${.ALLSRC} | ${NROFF} ${NOPTS} -Tascii > ${.TARGET}
.endfor

.include <bsd.regress.mk>
