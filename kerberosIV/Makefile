# $OpenBSD: Makefile,v 1.33 2001/07/03 18:33:03 matthieu Exp $

SUBDIR	= lib usr.bin usr.sbin libexec doc

build:
	${SUDO} ${MAKE} includes
	${MAKE} depend
	cd ${.CURDIR}/lib && ${MAKE} && ${SUDO} ${MAKE} install
	${MAKE}
	${SUDO} ${MAKE} install

.include <bsd.subdir.mk>
