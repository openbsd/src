# $OpenBSD: Makefile,v 1.30 2000/08/31 19:48:56 hin Exp $

SUBDIR	= lib usr.bin usr.sbin libexec

build:
	${MAKE} includes
	${MAKE} depend
	( cd ${.CURDIR}/lib && ${MAKE} && ${MAKE} install )
	${MAKE}
	${MAKE} install

.include <bsd.subdir.mk>
