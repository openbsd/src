#	$OpenBSD: Makefile,v 1.2 2001/02/03 08:14:59 niklas Exp $


DIR=	papers/newvm
SRCS=	0.t 1.t a.t
MACROS=	-ms

paper.ps: ${SRCS}
	${TBL} ${SRCS} | ${ROFF} > ${.TARGET}

.include <bsd.doc.mk>
