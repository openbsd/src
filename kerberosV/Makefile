# $OpenBSD: Makefile,v 1.9 2002/06/11 17:24:00 hin Exp $

SUBDIR	= usr.bin/asn1_compile lib usr.bin libexec usr.sbin doc

build:
	cd ../kerberosIV/lib/sl && exec ${MAKE}
	cd lib/roken && exec ${MAKE}
	cd usr.bin/asn1_compile && ${MAKE} && exec ${SUDO} ${MAKE} install
	${SUDO} ${MAKE} includes
	${MAKE} depend
	cd lib && ${MAKE} && exec ${SUDO} ${MAKE} install
	${MAKE}
	${SUDO} ${MAKE} install

.include <bsd.subdir.mk>
