#	$OpenBSD: Makefile,v 1.3 2004/02/01 14:22:44 jmc Exp $


DIR=	papers/kerntune
SRCS=	0.t 1.t 2.t 3.t 4.t
MACROS=	-ms

paper.ps: ${SRCS}
	${SOELIM} ${SRCS} | ${PIC} | ${TBL} | ${EQN} | ${ROFF} > ${.TARGET}

paper.txt: ${SRCS}
	${SOELIM} ${SRCS} | ${PIC} | ${TBL} | ${EQN} | \
		${ROFF} -Tascii > ${.TARGET}

.include <bsd.doc.mk>
