#	From: @(#)Makefile	6.3 (Berkeley) 6/8/93
#	$OpenBSD: Makefile,v 1.3 2004/02/01 14:22:44 jmc Exp $

VOLUME=	papers
DOC=	malloc
SRCS=	abs.ms intro.ms kernel.ms malloc.ms problems.ms alternatives.ms
SRCS+=	performance.ms implementation.ms conclusion.ms
MACROS=	-ms

paper.txt: ${SRCS}
	${ROFF} -Tascii ${SRCS} > ${.TARGET}

edit:
	vi ${SRCS}

.include <bsd.doc.mk>
