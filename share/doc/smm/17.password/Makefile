#	$OpenBSD: Makefile,v 1.1 2003/11/10 20:32:17 otto Exp $

DIR=	smm/17.password
SRCS=	password.ms
MACROS=	-ms

paper.ps: ${SRCS}
	${TBL} ${SRCS} | ${EQN} | ${ROFF} > ${.TARGET}

.include <bsd.doc.mk>
