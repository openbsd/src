#	$OpenBSD: Makefile,v 1.26 1999/03/09 00:01:42 niklas Exp $
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

LIBDIRS=acl krb kadm kafs kdb

.include <bsd.own.mk> # for AFS

.if (${AFS} == "yes")
SUBDIR+=afslog
.endif

build:
	${MAKE} cleandir
	${MAKE} depend
	${SUDO} ${MAKE} includes
	for dir in ${LIBDIRS}; do \
		(cd ${.CURDIR}/$$dir && ${MAKE} && ${SUDO} ${MAKE} install) \
	done

.include <bsd.dep.mk>
.include <bsd.subdir.mk>
