#	$OpenBSD: Makefile,v 1.22 1998/03/12 17:49:39 art Exp $
#	from @(#)Makefile	5.1 (Berkeley) 6/25/90

SUBDIR=

.ifmake includes
SUBDIR+=include
.endif

SUBDIR+=acl krb kadm kafs kdb

SUBDIR+=ext_srvtab kadmin kadmind kdb_destroy kdb_init kdb_edit \
	kdb_util kdestroy kerberos kinit klist kprop kpropd ksrvtgt \
	ksrvutil kstash kauth kauthd

SUBDIR+=man doc

.include <bsd.own.mk> # for AFS

.if (${AFS} == "yes")
SUBDIR+=afslog
.endif

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
