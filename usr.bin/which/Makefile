#	from: @(#)Makefile	5.5 (Berkeley) 7/1/90
#	$Id: Makefile,v 1.1.1.1 1995/10/18 08:46:52 deraadt Exp $

MAN=	which.1

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/which.csh ${DESTDIR}${BINDIR}/which

.include <bsd.prog.mk>
