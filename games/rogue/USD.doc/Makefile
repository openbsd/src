#	$OpenBSD: Makefile,v 1.2 2001/01/28 23:41:48 niklas Exp $
#	$NetBSD: Makefile,v 1.2 1995/04/22 10:28:47 cgd Exp $

DIR=	usd/30.rogue
SRCS=	rogue.me
MACROS=	-me

paper.${PRINTER}: ${SRCS}
	${TBL} ${SRCS} | ${ROFF} > ${.TARGET}

.include <bsd.doc.mk>
