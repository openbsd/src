#	@(#)Makefile	5.2 (Berkeley) 6/8/93

DIR=	papers/jus
SRCS=	paper.ms
MACROS=	-ms

paper.txt: ${SRCS}
	${ROFF} -Tascii ${SRCS} > ${.TARGET}

.include <bsd.doc.mk>
