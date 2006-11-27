# $OpenBSD: Makefile,v 1.3 2006/11/27 23:27:46 deraadt Exp $

NOMAN=chris is a lazy slacker

beforeinstall: 
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/pkg-config ${DESTDIR}${BINDIR}/pkg-config

.include <bsd.prog.mk>
