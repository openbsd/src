#	$OpenBSD: Makefile,v 1.5 1997/09/21 11:50:44 deraadt Exp $

MAN=	shar.1

beforeinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/shar.sh ${DESTDIR}/usr/bin/shar

.include <bsd.prog.mk>
