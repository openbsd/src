#	$OpenBSD: Makefile,v 1.1 2001/05/21 16:47:51 brian Exp $

NOPROG=
MAN=	pkg_version.1

afterinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/pkg_version.pl ${DESTDIR}${BINDIR}/pkg_version

.include <bsd.prog.mk>
