#	$NetBSD: Makefile,v 1.6 1994/12/22 12:30:52 cgd Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/6/93

MAN=	shar.1

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/shar.sh ${DESTDIR}/usr/bin/shar

.include <bsd.prog.mk>
