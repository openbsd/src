# $OpenBSD: Makefile,v 1.28 2000/02/25 16:35:22 hin Exp $

SUBDIR	= lib usr.bin usr.sbin libexec

build:
	${MAKE} cleandir
	${MAKE} includes
	${MAKE} depend
	( cd ${.CURDIR}/lib && ${MAKE} && ${MAKE} install )
	${MAKE}
	${MAKE} install

prereq:
	( cd ${.CURDIR}/lib/kadm && ${MAKE} prereq )
	( cd ${.CURDIR}/lib/krb && ${MAKE} prereq )

.include <bsd.subdir.mk>
