# $OpenBSD: Makefile,v 1.34 2003/05/14 01:22:56 hin Exp $

SUBDIR	= lib

build:
	${SUDO} ${MAKE} includes
	${MAKE} depend
	cd ${.CURDIR}/lib && ${MAKE} && ${SUDO} ${MAKE} install
	${MAKE}
	${SUDO} ${MAKE} install

.include <bsd.subdir.mk>
