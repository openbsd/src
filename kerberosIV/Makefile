#	$OpenBSD: Makefile,v 1.3 1996/05/15 08:51:59 niklas Exp $
#	from @(#)Makefile	5.1 (Berkeley) 6/25/90

# do compile_et and mk_cmds first, used by other directories

.ifmake install
SUBDIR=	include
.else
SUBDIR=	compile_et mk_cmds
.endif

SUBDIR+=acl com_err des krb kadm kafs kdb ss

SUBDIR+=ext_srvtab kadmin kadmind kdb_destroy kdb_edit kdb_init kdb_util \
	kdestroy kerberos kinit klist kpasswdd kprop kpropd ksrvtgt ksrvutil \
	kstash make_keypair register registerd

SUBDIR+=man

build:
	(cd ${.CURDIR}/include && ${MAKE} install)
	${MAKE} cleandir
	(cd ${.CURDIR}/compile_et && ${MAKE} depend && ${MAKE})
	(cd ${.CURDIR}/mk_cmds && ${MAKE} depend && ${MAKE})
	(cd ${.CURDIR}/acl && ${MAKE} depend && ${MAKE} && ${MAKE} install)
	(cd ${.CURDIR}/com_err && ${MAKE} depend && ${MAKE} && ${MAKE} install)
	(cd ${.CURDIR}/des && ${MAKE} depend && ${MAKE} && ${MAKE} install)
	(cd ${.CURDIR}/krb && ${MAKE} depend && ${MAKE} && ${MAKE} install)
	(cd ${.CURDIR}/kadm && ${MAKE} depend && ${MAKE} && ${MAKE} install)
	(cd ${.CURDIR}/kafs && ${MAKE} depend && ${MAKE} && ${MAKE} install)
	(cd ${.CURDIR}/kdb && ${MAKE} depend && ${MAKE} && ${MAKE} install)
	(cd ${.CURDIR}/ss && ${MAKE} depend && ${MAKE} && ${MAKE} install)

# Since depending need to have some dynamically built sources available,
# we need to build the generators first.
beforedepend:
	(cd ${.CURDIR}/compile_et && ${MAKE} depend && ${MAKE})
	(cd ${.CURDIR}/mk_cmds && ${MAKE} depend && ${MAKE})

.include <bsd.dep.mk>
.include <bsd.subdir.mk>
