#	$OpenBSD: Makefile,v 1.3 2004/02/01 15:24:03 jmc Exp $

DIR=	usd/16.awk
SRCS=	awk
MACROS=	-ms

paper.ps: ${SRCS}
	${TBL} ${SRCS} | ${ROFF} > ${.TARGET}

paper.txt: ${SRCS}
	${TBL} ${SRCS} | ${ROFF} -Tascii > ${.TARGET}

.include <bsd.doc.mk>
