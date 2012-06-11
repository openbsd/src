# $OpenBSD: Makefile,v 1.5 2012/06/11 10:16:46 espie Exp $

MAN=pkg-config.1

LIBDIR = /usr/libdata/perl5/OpenBSD

realinstall: 
	${INSTALL} -d -o ${LIBOWN} -g ${LIBGRP} -m ${DIRMODE} \
		${DESTDIR}${LIBDIR}
	${INSTALL} ${INSTALL_COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
		${.CURDIR}/OpenBSD/PkgConfig.pm ${DESTDIR}${LIBDIR}
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/pkg-config ${DESTDIR}${BINDIR}/pkg-config

NOPROG = 
.include <bsd.prog.mk>
