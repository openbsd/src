#	$OpenBSD: Makefile,v 1.2 2004/02/01 15:24:03 jmc Exp $

DIR=	usd/15.sed
SRCS=	sed
MACROS=	-ms

paper.txt: ${SRCS}
	${ROFF} -Tascii ${SRCS} > ${.TARGET}

.include <bsd.doc.mk>
