/*	$Id: site.h,v 1.3 1996/01/29 19:18:40 tholo Exp $	*/

/* 
 * Site-specific definitions.
 */

#ifndef SITE_H
#define SITE_H

/*
 * Location of common files.
 */
#define	KRB_CONF	"/etc/kerberosIV/krb.conf"
#define	KRB_RLM_TRANS	"/etc/kerberosIV/krb.realms"
#define KRB_ACL		"/etc/kerberosIV/kerberos.acl"
#define MKEYFILE	"/etc/kerberosIV/master_key"
#define KEYFILE		"/etc/kerberosIV/srvtab"
#define	DBM_FILE	"/etc/kerberosIV/principal"

#define K_LOGFIL	"/var/log/kpropd.log"
#define KS_LOGFIL	"/var/log/kerberos_slave.log"
#define KRBLOG 		"/var/log/kerberos.log"  /* master server  */
#define KRBSLAVELOG	"/var/log/kerberos_slave.log" /* master (?) server  */

/* from: kadm_server.h  */
/* the default syslog file */
#define KADM_SYSLOG	"/var/log/admin_server.log"

#define DEFAULT_ACL_DIR	"/etc/kerberosIV/"
/* These get appended to DEFAULT_ACL_DIR */
#define	ADD_ACL_FILE		"admin_acl.add"
#define	GET_ACL_FILE		"admin_acl.get"
#define	MOD_ACL_FILE		"admin_acl.mod"

/*
 * Set ORGANIZATION to be the desired organization string printed
 * by the 'kinit' program.  It may have spaces.
 */
#define ORGANIZATION	"The OpenBSD Project"

#endif
