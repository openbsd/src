#	$OpenBSD: Makefile,v 1.1 2003/09/19 18:03:13 otto Exp $

DIR=	usd/05.dc
SRCS=	dc
MACROS=	-ms

paper.ps: ${SRCS}
	${EQN} ${SRCS} | ${ROFF} > ${.TARGET}

.include <bsd.doc.mk>
