#	$OpenBSD: Makefile,v 1.1 1996/09/28 05:58:34 downsj Exp $

MAN=   adduser.8 adduser_proc.8

beforeinstall:
	install ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/adduser.perl ${DESTDIR}${BINDIR}/adduser

.include <bsd.prog.mk>
