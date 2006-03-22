#	$Sendmail: Makefile,v 8.16 2006/01/05 22:03:31 ca Exp $

DIR=	smm/08.sendmailop
SRCS=	op.me
MACROS=	-me

all: op.ps

op.ps: ${SRCS}
	rm -f ${.TARGET}
	${PIC} ${SRCS} | ${EQN} | ${ROFF} > ${.TARGET}

paper.txt: ${SRCS}
	rm -f ${.TARGET}
	${PIC} ${SRCS} | ${EQN} -Tascii | ${ROFF} -Tascii > ${.TARGET}

.include <bsd.doc.mk>
