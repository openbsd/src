#	$OpenBSD: Makefile,v 1.2 1997/04/27 20:55:57 millert Exp $

MAN=	sendbug.1

beforeinstall:
	install ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/sendbug.sh ${DESTDIR}${BINDIR}/sendbug

.include <bsd.prog.mk>
