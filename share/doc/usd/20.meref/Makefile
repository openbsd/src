#	$OpenBSD: Makefile,v 1.3 2004/02/01 14:22:46 jmc Exp $


DIR=	usd/20.meref
SRCS=	ref.me
MACROS=	-me

paper.txt: ${SRCS}
	${ROFF} -Tascii ${SRCS} > ${.TARGET}

.include <bsd.doc.mk>
