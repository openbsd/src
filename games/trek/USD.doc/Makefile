#	$NetBSD: Makefile,v 1.2 1995/04/22 10:59:48 cgd Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/8/93

DIR=	usd/31.trek
SRCS=	trek.me
MACROS=	-me

paper.ps: ${SRCS}
	${TBL} ${SRCS} | ${ROFF} > ${.TARGET}

.include <bsd.doc.mk>
