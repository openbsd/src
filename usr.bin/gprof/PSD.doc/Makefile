#	$OpenBSD: Makefile,v 1.3 2004/02/01 14:46:26 jmc Exp $
#	$NetBSD: Makefile,v 1.3 1995/04/19 07:16:35 cgd Exp $

DIR=	psd/18.gprof
SRCS=	header.me abstract.me intro.me profiling.me gathering.me \
	postp.me present.me refs.me
EXTRA=	postp1.pic postp2.pic postp3.pic pres1.pic pres2.pic
DPADD=	${EXTRA}
MACROS=	-me

paper.ps: ${SRCS} ${DPADD}
	${SOELIM} ${SRCS} | ${PIC} | ${TBL} | ${EQN} | ${ROFF} > ${.TARGET}

paper.txt: ${SRCS} ${DPADD}
	${SOELIM} ${SRCS} | ${PIC} | ${TBL} | ${EQN} -Tascii | \
		${ROFF} -Tascii > ${.TARGET}

.include <bsd.doc.mk>
