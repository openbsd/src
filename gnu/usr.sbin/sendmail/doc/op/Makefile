#	@(#)Makefile	8.2 (Berkeley) 2/28/1994

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
