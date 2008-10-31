#	$OpenBSD: Makefile,v 1.1 2008/10/31 14:11:04 jmc Exp $

MAN=	mdoclint.1

afterinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/mdoclint ${DESTDIR}/usr/bin/mdoclint

.include <bsd.prog.mk>
