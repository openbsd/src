# $OpenBSD: Makefile,v 1.1 2001/05/25 08:24:12 hin Exp $

SUBDIR	= usr.bin/asn1_compile lib usr.bin libexec usr.sbin

build:
	( cd lib/roken && ${MAKE} )
	( cd usr.bin/asn1_compile && ${MAKE} && ${MAKE} install )
	${MAKE} includes
	${MAKE} depend
	( cd lib && ${MAKE} && ${MAKE} install )
	${MAKE}
	${MAKE} install

.include <bsd.subdir.mk>
