#	$OpenBSD: Makefile,v 1.3 2004/02/01 14:22:45 jmc Exp $


DIR=	psd/20.ipctut
SRCS=	tutor.me
MACROS=	-me
EXTRA=	dgramread.c dgramsend.c fig2.pic fig3.pic fig8.pic pipe.c \
	socketpair.c strchkread.c streamread.c streamwrite.c \
	udgramread.c udgramsend.c ustreamread.c ustreamwrite.c

paper.ps: ${SRCS} ${EXTRA}
	${SOELIM} ${SRCS} | ${PIC} | ${TBL} | ${ROFF} > ${.TARGET}

paper.txt: ${SRCS} ${EXTRA}
	${SOELIM} ${SRCS} | ${PIC} | ${TBL} | ${ROFF} -Tascii > ${.TARGET}

.include <bsd.doc.mk>
