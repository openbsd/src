#	$OpenBSD: Makefile,v 1.4 2004/02/01 14:43:10 jmc Exp $


DIR=	psd/17.m4
SRCS=	m4.ms
MACROS=	-ms

paper.txt: ${SRCS}
	${ROFF} -Tascii ${SRCS} > ${.TARGET}

.include <bsd.doc.mk>
