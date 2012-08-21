#       $OpenBSD: Makefile,v 1.2 2012/08/21 14:46:19 pascal Exp $

NOMACHINE=
FILES=	bsd.tbxi 
NOOBJ=	noobj
NOPIE=

install:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 ${FILES} \
	    ${DESTDIR}/usr/mdec

.include <bsd.prog.mk>
