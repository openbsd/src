# $OpenBSD: Makefile,v 1.3 2001/06/05 10:06:20 hin Exp $

SUBDIR	= usr.bin/asn1_compile lib usr.bin libexec usr.sbin

build:
	cd lib/roken && exec ${MAKE}
	cd usr.bin/asn1_compile && ${MAKE} && exec ${SUDO} ${MAKE} install
	${SUDO} ${MAKE} includes
	${MAKE} depend
	cd lib && ${MAKE} && exec ${SUDO} ${MAKE} install
	${MAKE}
	${SUDO} ${MAKE} install

.include <bsd.subdir.mk>
