#	$OpenBSD: Makefile,v 1.2 1996/06/26 05:39:12 deraadt Exp $
#	$NetBSD: Makefile,v 1.6 1994/12/22 12:30:52 cgd Exp $

MAN=	shar.1

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/shar.sh ${DESTDIR}/usr/bin/shar

.include <bsd.prog.mk>
