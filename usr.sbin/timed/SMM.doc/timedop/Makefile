#	$OpenBSD: Makefile,v 1.2 2004/02/01 15:01:53 jmc Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/8/93

DIR=	smm/11.timedop
SRCS=	timed.ms
MACROS=	-ms

paper.txt: ${SRCS}
	${ROFF} -Tascii ${SRCS} > ${.TARGET}

.include <bsd.doc.mk>
