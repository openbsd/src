#	$OpenBSD: Makefile,v 1.1.1.1 1998/09/20 23:11:04 ian Exp $
#	@(#) This version did not come via Berkeley, but direct from Bell Labs.

DIR=	usd/02.learn
SRCS=	learn.ms
FILES=	COPYRIGHT $(SRCS)
MACROS=	-ms

paper.ps: ${SRCS}
	${TBL} ${SRCS} | ${ROFF} > ${.TARGET}

.include <bsd.doc.mk>
