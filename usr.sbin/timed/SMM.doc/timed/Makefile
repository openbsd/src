#	$OpenBSD: Makefile,v 1.3 2004/02/01 15:01:52 jmc Exp $
#	@(#)Makefile	8.2 (Berkeley) 5/11/94

DIR=	smm/12.timed
SRCS=	timed.ms
.if make(install)
SRCS+=	date loop time timed.ms unused
.endif
MACROS=	-ms

paper.ps: ${SRCS}
	${SOELIM} -I${.CURDIR} ${.ALLSRC} | ${TBL} | ${ROFF} > ${.TARGET}

paper.txt: ${SRCS}
	${SOELIM} -I${.CURDIR} ${.ALLSRC} | ${TBL} | \
		${ROFF} -Tascii > ${.TARGET}

.include <bsd.doc.mk>
