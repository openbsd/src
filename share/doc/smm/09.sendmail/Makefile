#	$OpenBSD: Makefile,v 1.2 2001/02/03 08:15:08 niklas Exp $


DIR=	smm/09.sendmail
SRCS=	intro.me
MACROS=	-me

all: intro.ps

intro.ps: ${SRCS}
	rm -f ${.TARGET}
	${PIC} ${SRCS} | ${ROFF} > ${.TARGET}

.include <bsd.doc.mk>
