#	$OpenBSD: Makefile,v 1.1 2003/06/26 16:20:04 mickey Exp $

DIR=	usd/16.awk
SRCS=	awk
MACROS=	-ms
REFER= refer -e -p /usr/dict/papers/Ind

paper.ps: ${SRCS}
	${REFER} ${SRCS} | ${TBL} | ${ROFF} > ${.TARGET}

.include <bsd.doc.mk>
