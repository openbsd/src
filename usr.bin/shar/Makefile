#	$OpenBSD: Makefile,v 1.4 1997/04/27 20:56:53 millert Exp $
#	$NetBSD: Makefile,v 1.6 1994/12/22 12:30:52 cgd Exp $

MAN=	shar.1

beforeinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/shar.sh ${DESTDIR}/usr/bin/shar

.include <bsd.prog.mk>
