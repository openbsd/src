#	$OpenBSD: Makefile,v 1.3 2003/06/26 16:16:01 mickey Exp $


DIR=	psd/17.m4
SRCS=	m4.ms
MACROS=	-ms

paper.ps: ${SRCS}
	${ROFF} ${SRCS} > ${.TARGET}

.include <bsd.doc.mk>
