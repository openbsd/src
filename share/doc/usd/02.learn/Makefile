#	$OpenBSD: Makefile,v 1.2 2004/02/01 14:22:45 jmc Exp $
#	@(#) This version did not come via Berkeley, but direct from Bell Labs.

DIR=	usd/02.learn
SRCS=	learn.ms
FILES=	COPYRIGHT $(SRCS)
MACROS=	-ms

paper.ps: ${SRCS}
	${TBL} ${SRCS} | ${ROFF} > ${.TARGET}

paper.txt: ${SRCS}
	${TBL} ${SRCS} | ${ROFF} -Tascii > ${.TARGET}

.include <bsd.doc.mk>
