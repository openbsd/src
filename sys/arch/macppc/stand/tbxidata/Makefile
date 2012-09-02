#       $OpenBSD: Makefile,v 1.3 2012/09/02 23:07:26 deraadt Exp $

NOMACHINE=
FILES=	bsd.tbxi 
NOOBJ=	noobj

install:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 ${FILES} \
	    ${DESTDIR}/usr/mdec

.include <bsd.prog.mk>
