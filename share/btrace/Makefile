#	$OpenBSD: Makefile,v 1.1 2021/10/23 19:37:35 mpi Exp $

FILES=	kprofile.bt runqlat.bt
NOOBJ=	noobj

install:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 ${FILES} \
	    ${DESTDIR}${BINDIR}/btrace

.include <bsd.prog.mk>
