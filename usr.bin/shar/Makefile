#	$OpenBSD: Makefile,v 1.3 1996/12/08 14:32:33 downsj Exp $
#	$NetBSD: Makefile,v 1.6 1994/12/22 12:30:52 cgd Exp $

MAN=	shar.1

beforeinstall:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/shar.sh ${DESTDIR}/usr/bin/shar

.include <bsd.prog.mk>
