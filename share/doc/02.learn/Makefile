#	$OpenBSD: Makefile,v 1.1.1.1 1998/09/20 22:21:50 ian Exp $
#	@(#) This version did not come via Berkeley, but direct from Bell Labs.

DIR=	usd/02.learn
SRCS=	COPYRIGHT learn.ms
MACROS=	-ms

paper.ps: ${SRCS}
	${TBL} ${SRCS} | ${ROFF} > ${.TARGET}

.include <bsd.doc.mk>
