#	@(#)Makefile	8.1 (Berkeley) 7/27/93

DIR=	smm/01.setup
SRCS=	0.t 1.t 2.t 3.t 4.t 5.t 6.t
FILES=	${SRCS}
MACROS=	-ms

paper.ps: ${SRCS}
	${TBL} ${SRCS} | ${ROFF} > ${.TARGET}

install: ${SRCS}
	install -c -o ${DOCOWN} -g ${DOCGRP} -m ${DOCMODE} \
	    Makefile ${FILES} ${EXTRA} ${DESTDIR}${DOCDIR}/${DIR}

.include <bsd.doc.mk>
