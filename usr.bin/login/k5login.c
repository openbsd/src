/*	$OpenBSD: k5login.c,v 1.2 1996/06/26 05:36:00 deraadt Exp $	*/
/*	$NetBSD: k5login.c,v 1.2 1994/12/23 06:52:58 jtc Exp $	*/

/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
#if 0
static char sccsid[] = "@(#)klogin.c	5.11 (Berkeley) 7/12/92";
#endif
static char rcsid[] = "$OpenBSD: k5login.c,v 1.2 1996/06/26 05:36:00 deraadt Exp $";
#endif /* not lint */

#ifdef KERBEROS5
#include <sys/param.h>
#include <sys/syslog.h>
#include <com_err.h>
#include <krb5/krb5.h>
#include <krb5/ext-proto.h>
#include <krb5/los-proto.h>
#include <pwd.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define KRB5_DEFAULT_OPTIONS 0
#define KRB5_DEFAULT_LIFE 60*60*10 /* 10 hours */

krb5_data tgtname = {
    KRB5_TGS_NAME_SIZE,
    KRB5_TGS_NAME
};

/*
 * Try no preauthentication first; then try the encrypted timestamp
 */
int preauth_search_list[] = {
	0,			
	KRB5_PADATA_ENC_TIMESTAMP,
	-1
	};

extern int notickets;
extern char *krbtkfile_env;
extern char *tty;

static char tkt_location[MAXPATHLEN];

/*
 * Attempt to log the user in using Kerberos authentication
 *
 * return 0 on success (will be logged in)
 *	  1 if Kerberos failed (try local password in login)
 */
int
klogin(pw, instance, localhost, password)
	struct passwd *pw;
	char *instance, *localhost, *password;
{
        krb5_error_code kerror;
	krb5_address **my_addresses;
	krb5_principal me, server;
	krb5_creds my_creds;
	krb5_timestamp now;
	krb5_ccache ccache = NULL;
	int preauth_type = -1;
	long lifetime = KRB5_DEFAULT_LIFE;
	int options = KRB5_DEFAULT_OPTIONS;
	int i;
	char *realm, *client_name;
	char *principal;
	
#ifdef SKEY
	/*
	 * We don't do s/key challenge and Kerberos at the same time
	 */
	if (strcasecmp(password, "s/key") == 0) {
	    return (1);
	}
#endif

	krb5_init_ets();

	/*
	 * Root logins don't use Kerberos.
	 * If we have a realm, try getting a ticket-granting ticket
	 * and using it to authenticate.  Otherwise, return
	 * failure so that we can try the normal passwd file
	 * for a password.  If that's ok, log the user in
	 * without issuing any tickets.
	 */
	if (strcmp(pw->pw_name, "root") == 0 ||
	    krb5_get_default_realm(&realm))
		return (1);

	/*
	 * get TGT for local realm
	 * tickets are stored in a file named TKT_ROOT plus uid
	 * except for user.root tickets.
	 */

	if (strcmp(instance, "root") != 0)
	    (void)sprintf(tkt_location, "FILE:/tmp/krb5cc_%d.%s",
		          pw->pw_uid, tty);
	else
	    (void)sprintf(tkt_location, "FILE:/tmp/krb5cc_root_%d.%s",
		          pw->pw_uid, tty);
	krbtkfile_env = tkt_location;

	principal = malloc(strlen(pw->pw_name)+strlen(instance)+2);
	strcpy(principal, pw->pw_name);
	if (strlen(instance)) {
	    strcat(principal, "/");
	    strcat(principal, instance);
	}
	
	if (kerror = krb5_cc_resolve(tkt_location, &ccache)) {
	    syslog(LOG_NOTICE, "warning: %s while getting default ccache",
		error_message(kerror));
	    return(1);
	}

	if (kerror = krb5_parse_name(principal, &me)) {
	    syslog(LOG_NOTICE, "warning: %s when parsing name %s",
		error_message(kerror), principal);
	    return(1);
	}
    
	if (kerror = krb5_unparse_name(me, &client_name)) {
	    syslog(LOG_NOTICE, "warning: %s when unparsing name %s",
		error_message(kerror), principal);
	    return(1);
	}

	kerror = krb5_cc_initialize (ccache, me);
	if (kerror != 0) {
	    syslog(LOG_NOTICE, "%s when initializing cache %s",
		error_message(kerror), tkt_location);
	    return(1);
	}

	memset((char *)&my_creds, 0, sizeof(my_creds));
    
	my_creds.client = me;

	if (kerror = krb5_build_principal_ext(&server,
					krb5_princ_realm(me)->length,
					krb5_princ_realm(me)->data,
					tgtname.length, tgtname.data,
					krb5_princ_realm(me)->length,
					krb5_princ_realm(me)->data,
					0)) {
	    syslog(LOG_NOTICE, "%s while building server name",
		error_message(kerror));
	    return(1);
	}

	my_creds.server = server;

	kerror = krb5_os_localaddr(&my_addresses);
	if (kerror != 0) {
	    syslog(LOG_NOTICE, "%s when getting my address",
		error_message(kerror));
	    return(1);
	}

	if (kerror = krb5_timeofday(&now)) {
	    syslog(LOG_NOTICE, "%s while getting time of day",
		error_message(kerror));
	    return(1);
	}
	my_creds.times.starttime = 0;	/* start timer when request
					   gets to KDC */
	my_creds.times.endtime = now + lifetime;
	my_creds.times.renew_till = 0;

	for (i=0; preauth_search_list[i] >= 0; i++) {
	    kerror = krb5_get_in_tkt_with_password(options, my_addresses,
		                                   preauth_search_list[i],
		                                   ETYPE_DES_CBC_CRC,
						   KEYTYPE_DES,
						   password,
						   ccache,
						   &my_creds, 0);
	    if (kerror != KRB5KDC_PREAUTH_FAILED &&
		kerror != KRB5KRB_ERR_GENERIC)
		    break;
	}

	krb5_free_principal(server);
	krb5_free_addresses(my_addresses);

	if (chown(&tkt_location[5], pw->pw_uid, pw->pw_gid) < 0)
		syslog(LOG_ERR, "chown tkfile (%s): %m", &tkt_location[5]);

	if (kerror) {
	    if (kerror == KRB5KRB_AP_ERR_BAD_INTEGRITY)
		printf("%s: Kerberos Password incorrect\n", principal);
	    else
		printf("%s while getting initial credentials\n", error_message(kerror));

	    return(1);
	}

	/* Success */
	notickets = 0;
	return(0);
}

/*
 * Remove any credentials
 */
void
kdestroy()
{
        krb5_error_code code;
	krb5_ccache ccache = NULL;

	if (krbtkfile_env == NULL)
	    return;

	code = krb5_cc_resolve(krbtkfile_env, &ccache);
	if (!code) {
	    code = krb5_cc_destroy(ccache);
	    if (!code) {
		krb5_cc_close(ccache);
	    }
	}
}
#endif
