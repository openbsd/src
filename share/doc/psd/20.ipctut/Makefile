#	$OpenBSD: Makefile,v 1.2 2001/02/03 08:15:04 niklas Exp $


DIR=	psd/20.ipctut
SRCS=	tutor.me
MACROS=	-me
EXTRA=	dgramread.c dgramsend.c fig2.pic fig3.pic fig8.pic pipe.c \
	socketpair.c strchkread.c streamread.c streamwrite.c \
	udgramread.c udgramsend.c ustreamread.c ustreamwrite.c

paper.ps: ${SRCS} ${EXTRA}
	${SOELIM} ${SRCS} | ${PIC} | ${TBL} | ${ROFF} > ${.TARGET}

.include <bsd.doc.mk>
