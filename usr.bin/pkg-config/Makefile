# $OpenBSD: Makefile,v 1.4 2006/11/28 00:48:08 ckuethe Exp $

MAN=pkg-config.1

beforeinstall: 
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/pkg-config ${DESTDIR}${BINDIR}/pkg-config

.include <bsd.prog.mk>
