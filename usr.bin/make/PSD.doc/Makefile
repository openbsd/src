#	$OpenBSD: Makefile,v 1.4 2004/02/01 14:33:13 jmc Exp $
#	$NetBSD: Makefile,v 1.2 1995/06/14 15:20:23 christos Exp $

DIR=	psd/12.make
SRCS=	tutorial.ms
MACROS=	-ms

paper.txt: ${SRCS}
	${ROFF} -Tascii ${SRCS} > ${.TARGET}

.include <bsd.doc.mk>
