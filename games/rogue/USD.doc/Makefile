#	$NetBSD: Makefile,v 1.2 1995/04/22 10:28:47 cgd Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/8/93

DIR=	usd/30.rogue
SRCS=	rogue.me
MACROS=	-me

paper.${PRINTER}: ${SRCS}
	${TBL} ${SRCS} | ${ROFF} > ${.TARGET}

.include <bsd.doc.mk>
