/*	$OpenBSD: login_krb5.c,v 1.17 2002/09/06 18:45:06 deraadt Exp $	*/

/*-
 * Copyright (c) 2001, 2002 Hans Insulander <hin@openbsd.org>.
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "common.h"

#include <kerberosV/krb5.h>
#ifdef KRB524
#include <kerberosIV/krb.h>
#endif

krb5_error_code ret;
krb5_context context;
krb5_ccache ccache;
krb5_principal princ;

void
krb5_syslog(krb5_context context, int level, krb5_error_code code, char *fmt, ...)
{
	va_list ap;
	char buf[256];

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	syslog(level, "%s: %s", buf, krb5_get_err_text(context, code));
}

void
store_tickets(struct passwd *pwd, int ticket_newfiles, int ticket_store,
    int token_install)
{
	char cc_file[MAXPATHLEN];
	krb5_ccache ccache_store;
#ifdef KRB524
	int get_krb4_ticket = 0;
	char krb4_ticket_file[MAXPATHLEN];
#endif

	if (ticket_newfiles)
		snprintf(cc_file, sizeof(cc_file), "FILE:/tmp/krb5cc_%d",
		    pwd->pw_uid);
	else
		snprintf(cc_file, sizeof(cc_file), "%s",
		    krb5_cc_default_name(context));

	if (ticket_store) {
		ret = krb5_cc_resolve(context, cc_file, &ccache_store);
		if (ret != 0) {
			krb5_syslog(context, LOG_ERR, ret,
			    "krb5_cc_gen_new");
			exit(1);
		}

		ret = krb5_cc_copy_cache(context, ccache, ccache_store);
		if (ret != 0)
			krb5_syslog(context, LOG_ERR, ret,
			    "krb5_cc_copy_cache");

		chown(krb5_cc_get_name(context, ccache_store),
		    pwd->pw_uid, pwd->pw_gid);

		fprintf(back, BI_SETENV " KRB5CCNAME %s:%s\n",
		    krb5_cc_get_type(context, ccache_store),
		    krb5_cc_get_name(context, ccache_store));

#ifdef KRB524
		get_krb4_ticket = krb5_config_get_bool_default (context,
		    NULL, get_krb4_ticket, "libdefaults",
		    "krb4_get_tickets", NULL);
		if (get_krb4_ticket) {
			CREDENTIALS c;
			krb5_creds cred;
			krb5_cc_cursor cursor;

			ret = krb5_cc_start_seq_get(context, ccache, &cursor);
			if (ret != 0) {
				krb5_syslog(context, LOG_ERR, ret,
				    "start seq");
				exit(1);
			}

			ret = krb5_cc_next_cred(context, ccache,
			    &cursor, &cred);
			if (ret != 0) {
				krb5_syslog(context, LOG_ERR, ret,
				    "next cred");
				exit(1);
			}

			ret = krb5_cc_end_seq_get(context, ccache,
			    &cursor);
			if (ret != 0) {
				krb5_syslog(context, LOG_ERR, ret,
				    "end seq");
				exit(1);
			}

			ret = krb524_convert_creds_kdc_ccache(context, ccache,
			    &cred, &c);
			if (ret != 0) {
				krb5_syslog(context, LOG_ERR, ret,
				    "convert");
			} else {
				snprintf(krb4_ticket_file,
				    sizeof(krb4_ticket_file),
				    "%s%d", TKT_ROOT, pwd->pw_uid);
				krb_set_tkt_string(krb4_ticket_file);
				tf_setup(&c, c.pname, c.pinst);
				chown(krb4_ticket_file,
				    pwd->pw_uid, pwd->pw_gid);
			}
		}
#endif
	}

	/* Need to chown the ticket file */
#ifdef KRB524
	if (get_krb4_ticket)
		fprintf(back, BI_SETENV " KRBTKFILE %s\n",
		    krb4_ticket_file);
#endif
}

int
krb5_login(char *username, char *invokinguser, char *password, int login,
    int tickets)
{
	int return_code = AUTH_FAILED;

	if (username == NULL || password == NULL)
		return (AUTH_FAILED);

	ret = krb5_init_context(&context);
	if (ret != 0) {
		krb5_syslog(context, LOG_ERR, ret, "krb5_init_context");
		exit(1);
	}

	ret = krb5_cc_gen_new(context, &krb5_mcc_ops, &ccache);
	if (ret != 0) {
		krb5_syslog(context, LOG_ERR, ret, "krb5_cc_gen_new");
		exit(1);
	}

	if (strcmp(username, "root") == 0) {
		char *tmp;
		int len = strlen(invokinguser)+6;

		tmp = malloc(len);
		snprintf(tmp, len, "%s/root", invokinguser);
		ret = krb5_parse_name(context, tmp, &princ);
		free(tmp);
	} else
		ret = krb5_parse_name(context, username, &princ);
	if (ret != 0) {
		krb5_syslog(context, LOG_ERR, ret, "krb5_parse_name");
		exit(1);
	}

	ret = krb5_verify_user_lrealm(context, princ, ccache,
	    password, 1, NULL);

	switch (ret) {
	case 0: {
		struct passwd *pwd;

		pwd = getpwnam(username);
		if (pwd == NULL) {
			krb5_syslog(context, LOG_ERR, ret,
			    "%s: no such user", username);
			return (AUTH_FAILED);
		}
		fprintf(back, BI_AUTH "\n");
		store_tickets(pwd, login && tickets, login && tickets, login);
		return_code = AUTH_OK;
		break;
	}
	case KRB5KRB_AP_ERR_MODIFIED:
		/* XXX syslog here? */
	case KRB5KRB_AP_ERR_BAD_INTEGRITY:
		break;
	default:
		krb5_syslog(context, LOG_ERR, ret, "verify");
		break;
	}

	krb5_free_context(context);
	krb5_free_principal(context, princ);
	krb5_cc_close(context, ccache);

	return (return_code);
}
