#	$OpenBSD: Makefile,v 1.2 2001/02/03 08:14:58 niklas Exp $


DIR=	papers/kerntune
SRCS=	0.t 1.t 2.t 3.t 4.t
MACROS=	-ms

paper.ps: ${SRCS}
	${SOELIM} ${SRCS} | ${PIC} | ${TBL} | ${EQN} | ${ROFF} > ${.TARGET}

.include <bsd.doc.mk>
