#	$OpenBSD: Makefile,v 1.2 2001/02/03 08:15:01 niklas Exp $


DIR=	papers/relengr
SRCS=	0.t 1.t 2.t 3.t
MACROS=	-ms
EXTRA=	ref.bib tmac.srefs
REFER=	refer -n -e -l -s -p ref.bib

paper.ps: ${SRCS}
	${REFER} ${SRCS} | ${ROFF} > ${.TARGET}

.include <bsd.doc.mk>
