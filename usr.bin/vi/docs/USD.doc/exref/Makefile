#	$OpenBSD: Makefile,v 1.4 2004/01/30 23:14:26 jmc Exp $


DIR=	usd/13.ex
SRCS=	ex.rm
EXTRA=	ex.summary
MACROS=	-ms
CLEANFILES=summary.*

paper.ps: ${SRCS} summary.ps
	${ROFF} ${SRCS} > ${.TARGET}
paper.txt: ${SRCS} summary.txt
	${ROFF} -Tascii ${SRCS} > ${.TARGET}

summary.ps: ex.summary
	${TBL} ex.summary | ${ROFF} > ${.TARGET}
summary.txt: ex.summary
	${TBL} ex.summary | ${ROFF} -Tascii > ${.TARGET}

.include <bsd.doc.mk>
