#	$OpenBSD: Makefile,v 1.3 2004/02/01 14:22:45 jmc Exp $


DIR=	smm/05.fastfs
SRCS=	0.t 1.t 2.t 3.t 4.t 5.t 6.t
MACROS=	-ms

paper.ps: ${SRCS}
	${TBL} ${SRCS} | ${EQN} | ${ROFF} > ${.TARGET}

paper.txt: ${SRCS}
	${TBL} ${SRCS} | ${EQN} | ${ROFF} -Tascii > ${.TARGET}

.include <bsd.doc.mk>
