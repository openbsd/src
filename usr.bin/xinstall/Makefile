#	$OpenBSD: Makefile,v 1.8 2017/07/09 21:23:19 espie Exp $

PROG=	xinstall
MAN=	install.1

realinstall:
	${INSTALL} ${INSTALL_COPY} -S ${INSTALL_STRIP} \
	    -o ${BINOWN} -g ${BINGRP} \
	    -m ${BINMODE} ${PROG} ${DESTDIR}${BINDIR}/install

.include <bsd.prog.mk>
