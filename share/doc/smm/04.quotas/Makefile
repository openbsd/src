#	$OpenBSD: Makefile,v 1.3 2004/02/01 14:22:45 jmc Exp $


DIR=	smm/04.quotas
SRCS=	quotas.ms
MACROS=	-ms

paper.txt: ${SRCS}
	${ROFF} -Tascii ${SRCS} > ${.TARGET}

.include <bsd.doc.mk>
