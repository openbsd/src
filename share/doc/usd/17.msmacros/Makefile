#	$OpenBSD: Makefile,v 1.1 2003/06/26 16:33:36 mickey Exp $

DIR=	usd/17.msmacros
SRCS=	ms
MACROS=	-msU

paper.ps: ${SRCS}
	${SOELIM} ${SRCS} | ${PIC} | ${TBL} | ${ROFF} > ${.TARGET}

.include <bsd.doc.mk>
