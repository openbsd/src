# $OpenBSD: Makefile,v 1.2 2001/05/25 12:00:47 jakob Exp $

SUBDIR	= usr.bin/asn1_compile lib usr.bin libexec usr.sbin

build:
	( cd lib/roken && ${MAKE} )
	( cd usr.bin/asn1_compile && ${MAKE} && ${SUDO} ${MAKE} install )
	${SUDO} ${MAKE} includes
	${MAKE} depend
	( cd lib && ${MAKE} && ${SUDO} ${MAKE} install )
	${MAKE}
	${SUDO} ${MAKE} install

.include <bsd.subdir.mk>
