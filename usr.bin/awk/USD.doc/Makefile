#	$OpenBSD: Makefile,v 1.2 2003/12/14 16:00:37 jmc Exp $

DIR=	usd/16.awk
SRCS=	awk
MACROS=	-ms

paper.ps: ${SRCS}
	${TBL} ${SRCS} | ${ROFF} > ${.TARGET}

.include <bsd.doc.mk>
