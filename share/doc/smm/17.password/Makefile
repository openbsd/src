#	$OpenBSD: Makefile,v 1.2 2004/02/01 14:22:45 jmc Exp $

DIR=	smm/17.password
SRCS=	password.ms
MACROS=	-ms

paper.ps: ${SRCS}
	${TBL} ${SRCS} | ${EQN} | ${ROFF} > ${.TARGET}

paper.txt: ${SRCS}
	${TBL} ${SRCS} | ${EQN} -Tascii | ${ROFF} -Tascii > ${.TARGET}

.include <bsd.doc.mk>
