#	$OpenBSD: Makefile,v 1.2 2001/02/03 08:14:58 niklas Exp $


DIR=	papers/memfs
SRCS=	0.t 1.t
MACROS=	-ms
REFER=	refer -n -e -l -s -p ref.bib
EXTRA=	ref.bib A.t tmac.srefs
CLEANFILES=ref.bib.i A.gt paper.t

paper.ps: paper.t
	${ROFF} tmac.srefs paper.t > ${.TARGET}

paper.t: ${SRCS} ref.bib.i A.gt
	${REFER} ${SRCS} A.gt > ${.TARGET}

ref.bib.i: ref.bib
	${INDXBIB} ref.bib

A.gt: A.t
	${GRIND} < A.t > A.gt

.include <bsd.doc.mk>
