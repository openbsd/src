# $OpenBSD: Makefile,v 1.7 2001/06/27 08:24:49 deraadt Exp $

SUBDIR	= usr.bin/asn1_compile lib usr.bin libexec usr.sbin doc

build:
	cd ../kerberosIV/lib/sl && exec ${MAKE}
	cd lib/roken && exec ${MAKE}
	cd usr.bin/asn1_compile && ${MAKE} && exec ${SUDO} ${MAKE} install
	${SUDO} ${MAKE} includes
	cd ../kerberosIV/lib/kafs && ${MAKE} && exec ${MAKE} install
	${MAKE} depend
	cd lib && ${MAKE} && exec ${SUDO} ${MAKE} install
	${MAKE}
	${SUDO} ${MAKE} install

.include <bsd.subdir.mk>
