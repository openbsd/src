#	$OpenBSD: Makefile,v 1.3 2001/01/29 01:58:33 niklas Exp $


DIR=	usd/13.ex
SRCS=	ex.rm
MACROS=	-ms
CLEANFILES=summary.*

paper.ps: ${SRCS} summary.ps
	${ROFF} ${SRCS} > ${.TARGET}

summary.ps: ex.summary
	${TBL} ex.summary | ${ROFF} > ${.TARGET}

.include <bsd.doc.mk>
