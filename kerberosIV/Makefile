#	$OpenBSD: Makefile,v 1.12 1997/12/15 17:56:12 art Exp $
#	from @(#)Makefile	5.1 (Berkeley) 6/25/90

SUBDIR=

.ifmake includes
SUBDIR+=include
.endif

SUBDIR+=acl krb kadm kafs kdb sl

SUBDIR+=ext_srvtab kadmin kadmind kdb_destroy kdb_edit kdb_init kdb_util \
	kdestroy kerberos kinit klist kprop kpropd ksrvtgt ksrvutil \
	kstash make_keypair register registerd

SUBDIR+=man

build:
	(cd ${.CURDIR}/include && ${MAKE} includes)
	${MAKE} cleandir
	${MAKE} includes
	(cd ${.CURDIR}/acl && ${MAKE} depend && ${MAKE} && ${MAKE} install)
	(cd ${.CURDIR}/krb && ${MAKE} depend && ${MAKE} && ${MAKE} install)
	(cd ${.CURDIR}/kadm && ${MAKE} depend && ${MAKE} && ${MAKE} install)
	(cd ${.CURDIR}/kafs && ${MAKE} depend && ${MAKE} && ${MAKE} install)
	(cd ${.CURDIR}/kdb && ${MAKE} depend && ${MAKE} && ${MAKE} install)

.include <bsd.dep.mk>
.include <bsd.subdir.mk>
