# $OpenBSD: Makefile,v 1.11 2003/05/13 02:47:06 hin Exp $

SUBDIR	= lib usr.bin libexec usr.sbin doc

build:
	cd ../kerberosIV/lib/sl && exec ${MAKE}
	cd lib/roken && exec ${MAKE}
	${SUDO} ${MAKE} includes
	${MAKE} depend
	cd lib && ${MAKE} && exec ${SUDO} ${MAKE} install
	${MAKE}
	${SUDO} ${MAKE} install

.include <bsd.subdir.mk>
