#	from @(#)Makefile	5.1 (Berkeley) 6/25/90
#	$Id: Makefile,v 1.1.1.1 1995/12/14 06:52:32 tholo Exp $

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

.include <bsd.subdir.mk>
