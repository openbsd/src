#	$OpenBSD: Makefile,v 1.1 1996/06/27 21:09:06 tholo Exp $

MAN=	sendbug.1

beforeinstall:
	install ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/sendbug.sh ${DESTDIR}${BINDIR}/sendbug

.include <bsd.prog.mk>
