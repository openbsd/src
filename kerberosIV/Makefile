#	$OpenBSD: Makefile,v 1.25 1998/09/07 00:47:12 niklas Exp $
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
	(cd ${.CURDIR}/include && ${SUDO} ${MAKE} includes)
	${MAKE} cleandir
	${SUDO} ${MAKE} includes
	(cd ${.CURDIR}/acl && ${MAKE} depend && ${MAKE} && ${SUDO} ${MAKE} install)
	(cd ${.CURDIR}/krb && ${MAKE} depend && ${MAKE} && ${SUDO} ${MAKE} install)
	(cd ${.CURDIR}/kadm && ${MAKE} depend && ${MAKE} && ${SUDO} ${MAKE} install)
	(cd ${.CURDIR}/kafs && ${MAKE} depend && ${MAKE} && ${SUDO} ${MAKE} install)
	(cd ${.CURDIR}/kdb && ${MAKE} depend && ${MAKE} && ${SUDO} ${MAKE} install)

.include <bsd.dep.mk>
.include <bsd.subdir.mk>
