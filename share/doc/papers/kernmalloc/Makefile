#	$OpenBSD: Makefile,v 1.2 2001/02/03 08:14:57 niklas Exp $


DIR=	papers/kernmalloc
SRCS=	kernmalloc.t appendix.t
MACROS=	-ms

paper.ps: ${SRCS} alloc.fig usage.tbl
	${SOELIM} ${SRCS} | ${TBL} | ${PIC} | ${EQN} | ${GRIND} | \
	    ${ROFF} > ${.TARGET}

.include <bsd.doc.mk>
