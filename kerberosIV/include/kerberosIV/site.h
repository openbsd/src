/*	$OpenBSD: site.h,v 1.5 1999/08/20 11:00:32 art Exp $	*/

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

/* used by kdb_init.c */
/* The default expire time for principals created by kadmind */
/* The time "1104555599" gives a date of:  Sat Jan  1 04:59:59 2005 */
#define KDBINIT_EXPDATE		1104555599
#define KDBINIT_EXPDATE_TXT	"12/31/04"

#define DEFAULT_ACL_DIR	"/etc/kerberosIV/"
/* These get appended to DEFAULT_ACL_DIR */
#define	ADD_ACL_FILE		"admin_acl.add"
#define	GET_ACL_FILE		"admin_acl.get"
#define	MOD_ACL_FILE		"admin_acl.mod"
#define DEL_ACL_FILE		"admin_acl.del"

/*
 * Set ORGANIZATION to be the desired organization string printed
 * by the 'kinit' program.  It may have spaces.
 */
#define ORGANIZATION	"The OpenBSD Project"

#endif
