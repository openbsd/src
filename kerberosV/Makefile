# $OpenBSD: Makefile,v 1.5 2001/06/22 16:20:29 hin Exp $

SUBDIR	= usr.bin/asn1_compile lib usr.bin libexec usr.sbin doc

build:
	cd ../kerberosIV/lib/sl && exec ${MAKE}
	cd ../kerberosIV/lib/kafs && ${MAKE} && exec sudo ${MAKE} install
	cd lib/roken && exec ${MAKE}
	cd usr.bin/asn1_compile && ${MAKE} && exec ${SUDO} ${MAKE} install
	${SUDO} ${MAKE} includes
	${MAKE} depend
	cd lib && ${MAKE} && exec ${SUDO} ${MAKE} install
	${MAKE}
	${SUDO} ${MAKE} install

.include <bsd.subdir.mk>
