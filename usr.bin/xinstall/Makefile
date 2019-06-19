#	$OpenBSD: Makefile,v 1.9 2019/02/24 12:57:14 kn Exp $

PROG=	xinstall
MAN=	install.1

realinstall:
	${INSTALL} ${INSTALL_COPY} ${INSTALL_STRIP} \
	    -o ${BINOWN} -g ${BINGRP} \
	    -m ${BINMODE} ${PROG} ${DESTDIR}${BINDIR}/install

.include <bsd.prog.mk>
