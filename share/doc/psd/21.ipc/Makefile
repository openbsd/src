#	$OpenBSD: Makefile,v 1.2 2001/02/03 08:15:06 niklas Exp $


DIR=	psd/21.ipc
SRCS=	0.t 1.t 2.t 3.t 4.t 5.t
MACROS=	-ms

paper.ps: ${SRCS}
	${TBL} ${SRCS} | ${ROFF} > ${.TARGET}

.include <bsd.doc.mk>
