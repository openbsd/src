#       $OpenBSD: Makefile,v 1.1 2001/10/03 17:10:06 drahn Exp $

NOMACHINE=
FILES=	bsd.tbxi 
NOOBJ=	noobj

install:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 ${FILES} \
	    ${DESTDIR}/usr/mdec

.include <bsd.prog.mk>
