# $OpenBSD: Makefile,v 1.6 2001/06/23 22:53:22 hin Exp $

SUBDIR	= usr.bin/asn1_compile lib usr.bin libexec usr.sbin doc

build:
	cd ../kerberosIV/lib/sl && exec ${MAKE}
	cd lib/roken && exec ${MAKE}
	cd usr.bin/asn1_compile && ${MAKE} && exec ${SUDO} ${MAKE} install
	${SUDO} ${MAKE} includes
	cd ../kerberosIV/lib/kafs && ${MAKE} && exec sudo ${MAKE} install
	${MAKE} depend
	cd lib && ${MAKE} && exec ${SUDO} ${MAKE} install
	${MAKE}
	${SUDO} ${MAKE} install

.include <bsd.subdir.mk>
