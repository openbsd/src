#	$OpenBSD: Makefile,v 1.2 2001/02/03 08:15:08 niklas Exp $


DIR=	smm/05.fastfs
SRCS=	0.t 1.t 2.t 3.t 4.t 5.t 6.t
MACROS=	-ms

paper.ps: ${SRCS}
	${TBL} ${SRCS} | ${EQN} | ${ROFF} > ${.TARGET}

.include <bsd.doc.mk>
