#	$OpenBSD: Makefile,v 1.3 2004/02/01 14:22:45 jmc Exp $


DIR=	smm/06.nfs
SRCS=	0.t 1.t 2.t ref.t
MACROS=	-me

paper.txt: ${SRCS}
	${ROFF} -Tascii ${SRCS} > ${.TARGET}

.include <bsd.doc.mk>
